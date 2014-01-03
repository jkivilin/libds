/*
 * File input module
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

struct file_input_priv {
	int fd;
	char filename[MAXPATHLEN];
};

static bool file_input_close(struct io_input *input, void *__priv)
{
	struct file_input_priv *priv = __priv;
	int ret;

	if (priv->fd > -1) {
		ret = close(priv->fd);
		if (ret < 0)
			return false;

		priv->fd = -1;
	}

	return true;
}

static int file_input_open(struct io_input *input, void *__priv)
{
	struct file_input_priv *priv = __priv;
	int ret;

	if (priv->fd > -1) {
		ret = close(priv->fd);
		if (ret < 0) {
			io_set_latest_error("%s():%d: could not close fd[%d] "
					    "(errno: %d)", __func__, __LINE__,
					    priv->fd, errno);
			return false;
		}

		priv->fd = -1;
	}

	/* open read-only file */
	priv->fd = open(priv->filename, O_RDONLY);
	if (priv->fd < 0) {
		io_set_latest_error("%s():%d: could not open file[%s] "
				    "(errno: %d)", __func__, __LINE__,
				    priv->filename, errno);
		priv->fd = -1;
	}

	return priv->fd;
}

/**
 * io_new_file_input - allocate and initialize file input module
 * @parser: bottom of parser stack to use
 * @filename: file to open for input
 */
struct io_input *io_new_file_input(struct io_parser *parser,
				   const char *filename)
{
	struct file_input_priv *finput;
	struct io_input *io;

	if (!parser)
		return NULL;

	finput = calloc(1, sizeof(*finput));
	if (!finput) {
		io_set_latest_error("%s():%d: calloc failed (errno: %d)",
				    __func__, __LINE__, errno);
		return NULL;
	}

	/* Initialize */
	snprintf(finput->filename, sizeof(finput->filename), "%s", filename);
	finput->fd = -1;

	/* Initialize generic fd input module */
	io = io_new_generic_fd_input(parser, file_input_open, file_input_close,
				     finput);
	if (!io) {
		free(finput);
		return NULL;
	}

	return io;
}
