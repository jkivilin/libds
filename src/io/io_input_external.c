/*
 * External input module
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

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#include "io.h"

struct external_input_priv {
	struct io_input input;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct ds_append_buffer buf;
	bool has_new;
};

static inline struct external_input_priv *
				external_input_priv(struct io_input *input)
{
	return ds_container_of(input, struct external_input_priv, input);
}

static int external_input_read(struct io_input *input, int *error)
{
	struct external_input_priv *priv = external_input_priv(input);
	unsigned char buf[128];
	unsigned int rlen, wlen;
	int read_bytes = 0;

	pthread_mutex_lock(&priv->mutex);

	while (ds_append_buffer_length(&priv->buf) > 0) {
		/* Copy bytes from input buffer ... */
		rlen = ds_append_buffer_copy(&priv->buf, 0, buf, sizeof(buf));
		read_bytes += rlen;

		/* Move head pointer forward */
		ds_append_buffer_move_head(&priv->buf, rlen);

		/* Append bytes to output buffer ... */
		wlen = ds_append_buffer_append(&input->inbuf, buf, rlen);
		if (wlen < rlen) {
			/* out of memory? */
			read_bytes -= (rlen - wlen);
			break;
		}
	}

	pthread_mutex_unlock(&priv->mutex);

	return read_bytes;
}

enum io_input_wait_ret external_input_wait(struct io_input *input)
{
	struct external_input_priv *priv = external_input_priv(input);
	enum io_input_wait_ret ret = IO_INPUT_WAIT_NEW;

	pthread_mutex_lock(&priv->mutex);

	/* Check if new input was added ... */
	if (!priv->has_new) {
		/* Wait for input ... */
		pthread_cond_wait(&priv->cond, &priv->mutex);

		/* Recheck if new input was added ... */
		if (priv->has_new)
			ret = IO_INPUT_WAIT_NEW;
		else
			ret = IO_INPUT_WAIT_STOP;
	}

	priv->has_new = false;

	pthread_mutex_unlock(&priv->mutex);

	return ret;
}

static bool external_input_stop_wait(struct io_input *input)
{
	struct external_input_priv *priv = external_input_priv(input);

	/* wake up all waiters */
	pthread_cond_broadcast(&priv->cond);

	return true;
}

static bool external_input_destroy(struct io_input *input)
{
	struct external_input_priv *priv = external_input_priv(input);

	if (!io_parser_destroy(input->parser))
		return false;

	pthread_mutex_lock(&priv->mutex);
	ds_append_buffer_free(&priv->buf);
	pthread_mutex_unlock(&priv->mutex);

	pthread_cond_destroy(&priv->cond);
	pthread_mutex_destroy(&priv->mutex);

	io_input_free(&priv->input);
	free(priv);

	return true;
}

static bool external_input_reopen(struct io_input *input)
{
	/* no reopen for external input */
	return false;
}

static const struct io_input_ops external_input_ops = {
	.read = external_input_read,
	.wait = external_input_wait,
	.stop_wait = external_input_stop_wait,
	.destroy = external_input_destroy,
	.reopen = external_input_reopen,
};

/**
 * io_new_external_input - allocate and initialize external input module
 * @parser: bottom of parser stack to use
 */
struct io_input *io_new_external_input(struct io_parser *parser)
{
	struct external_input_priv *einput;

	if (!parser)
		return NULL;

	einput = calloc(1, sizeof(*einput));
	if (!einput) {
		io_set_latest_error("%s():%d: calloc failed (errno: %d)",
				    __func__, __LINE__, errno);
		io_parser_destroy(parser);
		return NULL;
	}

	/* Initialize */
	io_input_init(&einput->input, &external_input_ops, parser);

	ds_append_buffer_init(&einput->buf);
	pthread_mutex_init(&einput->mutex, NULL);
	pthread_cond_init(&einput->cond, NULL);

	return &einput->input;
}

/**
 * io_external_input_push_data - push new data to input system
 * @ext_input: input structure allocated with io_new_external_input()
 * @buf: buffer with new data
 * @buflen: length of buffer in bytes
 *
 * Returns number of bytes pushed (returns < buflen when out of memory).
 */
int io_external_input_push_data(struct io_input *ext_input, void *buf,
				unsigned int buflen)
{
	struct external_input_priv *priv = external_input_priv(ext_input);
	int alen;

	pthread_mutex_lock(&priv->mutex);
	alen = ds_append_buffer_append(&priv->buf, buf, buflen);
	if (alen < buflen) {
		/* out of memory! do we continue as nothing happened or break? */
		/* ignore non-mem for now */
		buflen = alen;
	}
	priv->has_new = true;
	pthread_mutex_unlock(&priv->mutex);

	/* wake up waiting thread */
	pthread_cond_signal(&priv->cond);

	return buflen;
}
