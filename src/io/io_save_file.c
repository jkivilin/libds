/*
 * IO library file saving support
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
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "priv_zlib.h"
#include "io.h"

struct io_save_ops {
	void *(*open)(const char *filename);
	bool (*close)(void *fp);
	int (*write)(void *fp, const void *buf, size_t buflen);
};

/*
 * Plain file saver
 */ 
static void *plain_open(const char *filename)
{
	int fd;

	fd = open(filename, O_CREAT | O_WRONLY, 
		  S_IRUSR | S_IWUSR | S_IRGRP |
		  S_IWGRP | S_IROTH | S_IWOTH);

	/* fd might be 0, offset by one */
	return (void *)((long)fd + 1);
}

static bool plain_close(void *fp)
{
	int fd = (long)fp - 1;

	return close(fd) == 0;
}

static int plain_write(void *fp, const void *buf, size_t buflen)
{
	int fd = (long)fp - 1;
	ssize_t wlen;
	size_t tlen = 0;

	/* write() doesn't quarantee that it will write all buflen bytes at
	 * once (althought it usually does).
	 */
	do {
		wlen = write(fd, buf, buflen);
		if (wlen == -1) {
			if (errno != EINTR)
				return -1;

			wlen = 0;
		}

		tlen += wlen;
		buf = (void *)((size_t)buf + wlen);
		buflen -= wlen;
	} while (buflen > 0);

	return tlen;
}

static struct io_save_ops plain_ops = {
	.open = plain_open,
	.close = plain_close,
	.write = plain_write
};

/*
 * GZIP file saver
 */ 
static void *gzip_open(const char *filename)
{
	return gzopen(filename, "wb9");
}

static bool gzip_close(void *fp)
{
	return gzclose_w(fp) == Z_OK;
}

static int gzip_write(void *fp, const void *buf, size_t buflen)
{
	if (buflen == 0)
		return 0;

	/* gzwrite returns zero on error, we want -1 */
	buflen = gzwrite(fp, buf, buflen);
	if (buflen == 0)
		return -1;

	return buflen;
}

static struct io_save_ops gzip_ops = {
	.open = gzip_open,
	.close = gzip_close,
	.write = gzip_write
};

/*
 * Delta encoder
 */
struct io_save_delta_encoder {
	float curr_value;
};

static bool delta_encoder_init(struct io_save_delta_encoder *enc, void *file,
			       struct io_save_ops *ops)
{
	int ret;

	/* init variables */
	enc->curr_value = 0;

	/* mark file as delta-encoded */
	ret = ops->write(file, "#deltaenc\n", strlen("#deltaenc\n"));
	if (ret < 0)
		return false;

	return true;
}

static int delta_encode(struct io_save_delta_encoder *enc, void *buf,
			size_t buflen, float value)
{
	float delta;
	int slen;

	/* get new delta */
	delta = value - enc->curr_value;

	/* write delta to buffer */
	slen = snprintf(buf, buflen, "%.3f\n", delta);

	/* read delta back from buffer (because lost accurancy/rounding) */
	sscanf(buf, "%f", &delta);

	/* adjust encoder value with delta */
	enc->curr_value += delta;

	return slen;
}

/*
 * Generic file saver function
 */
static bool io_save_generic_txt_file(const char *filename, float *values,
				     unsigned int num_values, bool use_gz)
{
	struct io_save_delta_encoder enc;
	struct io_save_ops *ops;
	void *file;
	unsigned int i;
	int ret;

	/* select functions to use based on whatever to use compression or not */
	ops = use_gz ? &gzip_ops : &plain_ops;

	file = ops->open(filename);
	if (!file) {
		io_set_latest_error("%s():%d: could not open filename[%s] "
				    "(errno: %d)", __func__, __LINE__, filename,
				    errno);
		return false;
	}

	if (!delta_encoder_init(&enc, file, ops)) {
		/* error? skip or what? */
		io_set_latest_error("%s():%d: delta_encoder_init failed "
				    "(errno: %d)", __func__, __LINE__, filename,
				    errno);
		ops->close(file);
		return false;
	}

	/* save data values to file */
	for (i = 0; i < num_values; i++) {
		char buf[32];
		int slen;

		slen = delta_encode(&enc, buf, sizeof(buf), values[i]);

		ret = ops->write(file, buf, slen);
		if (ret <= 0) {
			/* error? skip or what? */
			io_set_latest_error("%s():%d: ops->write failed "
					    "(ret: %d) (errno: %d)", __func__,
					    __LINE__, ret, errno);
			ops->close(file);
			return false;
		}
	}

	if (ops->close(file))
		return true;

	/* error at close, compression failed */
	io_set_latest_error("%s():%d: ops->close failed (errno: %d)", __func__,
			    __LINE__, errno);
	return false;
}

/**
 * io_save_txt_file - save @values to file with name @filename, using text
 *		      formating
 * @filename: filename to use
 * @values: data points to save (ecg data, 4ms intervals, 250pps)
 * @num_values: number of values
 *
 * Text format contains each data point on new line. Each data point is
 * presented as floating-point with two desimals.
 *
 * Example file:
 *  1.000
 *  2.000
 *  2.110
 *  0.220
 *  0.230
 *  0.260
 *
 * Data can also be saved delta-encoded, where first data point is saved as such
 * and following numbers are stored as delta of previous value. First line of
 * file will be "#deltaenc" for detection.
 * 
 * Above example file as delta-encoded:
 *  #deltaenc
 *  1.000
 *  1.000
 *  0.110
 *  -1.890
 *  0.010
 *  0.030
 */
bool io_save_txt_file(const char *filename, float *values,
		      unsigned int num_values)
{
	return io_save_generic_txt_file(filename, values, num_values, false);
}

/**
 * io_save_gz_txt_file - save @values to file with name @filename, using text
 *			 formating and gzip compression.
 * @filename: filename to use
 * @values: data points to save (ecg data, 4ms intervals, 250pps)
 * @num_values: number of values
 *
 * Text format is same as with io_save_txt_file()
 */
bool io_save_gz_txt_file(const char *filename, float *values,
			 unsigned int num_values)
{
	return io_save_generic_txt_file(filename, values, num_values, true);
}
