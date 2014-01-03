/*
 * Generic file descriptor input module
 *
 * Copyright © 2012-2013 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "io.h"

struct fd_input_priv {
	struct io_input input;

	int fd;
	int wait_sockets[2];
	void *priv;

	io_generic_fd_open open_func;
	io_generic_fd_close close_func;
};

static inline struct fd_input_priv *fd_input_priv(struct io_input *input)
{
	return ds_container_of(input, struct fd_input_priv, input);
}

static int fd_input_read(struct io_input *input, int *error)
{
	struct fd_input_priv *priv = fd_input_priv(input);
	struct ds_append_buffer *buffer = &input->inbuf;
	unsigned char *read_buf;
	unsigned int blen = 0;
	ssize_t rlen;
	int err;
	*error = 0;

	/* Avoid memory copies, read() directly to append_buffer... */
	read_buf = ds_append_buffer_get_write_buffer(buffer, &blen);
	if (!read_buf) {
		/* out of mem */
		return 0;
	}

retry:
	rlen = read(priv->fd, read_buf, blen);
	if (rlen < 0) {
		err = errno;

		/* non-blocking event */
		if (err == EAGAIN || err == EWOULDBLOCK) {
			err = 0;
			goto out;
		}
		/* ignore signals and retry */
		if (err == EINTR)
			goto retry;

		*error = err;
		err = -1;
		goto out;
	}

	/* end of file */
	if (rlen == 0) {
		*error = ENOSPC;
		err = -1;
		goto out;
	}

	/* append new read at end of appendable buffer */
	ds_append_buffer_finish_write_buffer(buffer, read_buf, rlen);

	return rlen;

out:
	/* free unused buffer */
	ds_append_buffer_finish_write_buffer(buffer, read_buf, 0);

	return err;
}

static enum io_input_wait_ret fd_input_wait(struct io_input *input)
{
	struct fd_input_priv *priv = fd_input_priv(input);
	struct pollfd fds[2];
	int ret, err;
	unsigned char tmp[16];
	bool interrupted;

	/* read all input from wait socket */
	do {
		interrupted = false;
		ret = read(priv->wait_sockets[1], tmp, sizeof(tmp));
		if (ret < 0) {
			err = errno;
			if (err == EAGAIN || err == EWOULDBLOCK)
				break;
			if (err == EINTR) {
				interrupted = true;
				continue;
			}

			return IO_INPUT_WAIT_ERROR;
		}
	} while (ret > 0 || interrupted);

	do {
		fds[0].fd = priv->fd;
		fds[0].events = POLLIN;
		fds[0].revents = 0;

		fds[1].fd = priv->wait_sockets[1];
		fds[1].events = POLLIN;
		fds[1].revents = 0;

		ret = poll(fds, 2, -1);
		if (ret < 0) {
			err = errno;
			if (err == EAGAIN || err == EINTR)
				continue;
		}

		if (ret == 0)
			continue;

		/* End of file or fd error */
		if (fds[0].revents & (POLLHUP | POLLERR))
			return IO_INPUT_WAIT_ERROR;

		/* new input data available */
		if (fds[0].revents)
			return IO_INPUT_WAIT_NEW;

		/* stop wait if any data received on wait socket */
	} while (!fds[1].revents);

	return IO_INPUT_WAIT_STOP;
}

static bool fd_input_stop_wait(struct io_input *input)
{
	struct fd_input_priv *priv = fd_input_priv(input);
	unsigned char s = 's';
	ssize_t wlen;

	/* Writing to wait socket to wake up poll() */
	wlen = write(priv->wait_sockets[0], &s, 1);

	return (wlen > 0);
}

static bool fd_input_destroy(struct io_input *input)
{
	struct fd_input_priv *priv = fd_input_priv(input);

	shutdown(priv->wait_sockets[0], SHUT_RDWR);
	shutdown(priv->wait_sockets[1], SHUT_RDWR);

	close(priv->wait_sockets[0]);
	close(priv->wait_sockets[1]);

	if (!priv->close_func(input, priv->priv))
		return false;

	priv->fd = -1;

	if (!io_parser_destroy(input->parser))
		return false;

	io_input_free(&priv->input);
	free(priv->priv);
	free(priv);

	return true;
}

static bool fd_input_reopen(struct io_input *input)
{
	struct fd_input_priv *priv = fd_input_priv(input);
	int new_fd;

	/* reset parser */
	if (!io_parser_reset(input->parser))
		return false;

	/* reopen file descriptor */
	new_fd = priv->open_func(&priv->input, priv->priv);
	if (new_fd < 0)
		return false;

	priv->fd = new_fd;
	return true;
}

static const struct io_input_ops fd_input_ops = {
	.read = fd_input_read,
	.wait = fd_input_wait,
	.stop_wait = fd_input_stop_wait,
	.destroy = fd_input_destroy,
	.reopen = fd_input_reopen,
};

/**
 * io_new_generic_fd_input - allocate and initialize file descriptor input
 *			     module
 * @parser: bottom of parser stack to use
 * @open_func: function for opening/reopening file descriptor
 * @close_func: function for closing file descriptor
 * @priv: private data for open_func
 */
struct io_input *io_new_generic_fd_input(struct io_parser *parser,
					 io_generic_fd_open open_func,
					 io_generic_fd_close close_func,
					 void *priv)
{
	struct fd_input_priv *finput;
	int i;

	if (!parser)
		return NULL;

	finput = calloc(1, sizeof(*finput));
	if (!finput) {
		io_set_latest_error("%s():%d: calloc failed (errno: %d)",
				    __func__, __LINE__, errno);
		goto err_destroy_parser;
	}

	/* Initialize */
	io_input_init(&finput->input, &fd_input_ops, parser);
	finput->fd = -1;
	finput->priv = priv;
	finput->open_func = open_func;
	finput->close_func = close_func;

	/* Setup up socketpair used for signal stop_wait to poll() */
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, finput->wait_sockets) < 0) {
		io_set_latest_error("%s():%d: could not create waiting "
				    "socketpair (errno: %d)", __func__,
				    __LINE__, errno);
		goto err_free;
	}

	/* make sockets non-blocking */
	for (i = 0; i < 2; i++) {
		if (!io_set_fd_nonblocking(finput->wait_sockets[i])) {
			io_set_latest_error("%s():%d: could not set socket[%d] "
					    "non-blocking (errno: %d)",
					    __func__, __LINE__, i, errno);
			goto err_close_sockets;
		}
	}

	/* open file descriptor */
	finput->fd = finput->open_func(&finput->input, finput->priv);
	if (finput->fd < 0)
		goto err_close_sockets;

	return &finput->input;

err_close_sockets:
	close(finput->wait_sockets[0]);
	close(finput->wait_sockets[1]);
err_free:
	io_input_free(&finput->input);
	free(finput);
err_destroy_parser:
	io_parser_destroy(parser);
	return NULL;
}
