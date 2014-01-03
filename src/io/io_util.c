/*
 * IO library utility functions
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
#include <unistd.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>

#include "io.h"

static struct {
	char error_str[512];
	char __zero;
} io_util;

/**
 * io_sleep_to - sleep to absolute timeout
 * @abstime: time in future to sleep to
 */
void io_sleep_to(struct ds_timespec *abstime)
{
	pthread_mutex_t tmp_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t tmp_cond = PTHREAD_COND_INITIALIZER;
	struct timespec time;

	time.tv_sec = abstime->tv_sec;
	time.tv_nsec = abstime->tv_nsec;

	pthread_mutex_lock(&tmp_mutex);
	pthread_cond_timedwait(&tmp_cond, &tmp_mutex, &time);
	pthread_mutex_unlock(&tmp_mutex);
}

/**
 * io_usleep - sleep for @usec microseconds
 */
void io_usleep(unsigned int usec)
{
	struct ds_timespec abstime;

	ds_make_timeout_us(&abstime, usec);

	io_sleep_to(&abstime);
}

/**
 * io_set_fd_nonblocking - set unix file/socket descriptor to non-blocking mode
 * @fd: file descriptor to modify
 *
 * Returns true if successfull
 */
bool io_set_fd_nonblocking(int fd)
{
	int ret, flags;

	/* read current flags */
	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		flags = 0;

	/* set flags with non-blocking enable */
	ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (ret < 0)
		return false;

	return true;
}

/**
 * (internal) io_set_latest_error - set error string for currently happened
 *				    error
 * @fmt: snprintf format string
 * @...: format arguments
 */
void io_set_latest_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(io_util.error_str, sizeof(io_util.error_str), fmt, ap);
	va_end(ap);
}

/**
 * io_get_latest_error - get error string of latest happened libio error
 *
 * Return pointer to static string buffer, not thread-safe!
 */
const char *io_get_latest_error(void)
{
	return io_util.error_str;
}
