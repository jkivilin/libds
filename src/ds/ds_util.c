/*
 * Helper functions
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
#include <time.h>
#include <sys/time.h>
#include <errno.h>

#include "ds.h"

/**
 * ds_get_curr_timespec - utility function for getting current system time
 * @ts: time data structure to store current time to
 */
extern int ds_get_curr_timespec(struct ds_timespec *ts)
{
	struct timeval tv;
	int ret;
	
        ret = gettimeofday(&tv, NULL);
	if (ret == -1)
		return -errno;

        ts->tv_sec = tv.tv_sec;
        ts->tv_nsec = tv.tv_usec * 1000;
	
	return 0;
}

/**
 * ds_add_timespec_usec - utility function for adding useconds to ds_timespec
 * @time: time structure
 * @usec: time in microseconds to add to @time
 */
void ds_add_timesec_usec(struct ds_timespec *time, unsigned int usec)
{
	time->tv_nsec += (unsigned long long)usec * 1000;
	while (time->tv_nsec > 1000ULL * 1000 * 1000) {
		time->tv_nsec -= 1000ULL * 1000 * 1000;
		time->tv_sec++;
	}
}

/**
 * ds_make_timeout_us - utility function from making absolute timeout from
 *			relative microsecond timeout
 * @abstimeout: absolute time of timeout
 * @timeout_us: relative timeout in microseconds
 */
int ds_make_timeout_us(struct ds_timespec *abstimeout, unsigned int timeout_us)
{
	int ret;

	ret = ds_get_curr_timespec(abstimeout);
	if (ret)
		return ret;

	ds_add_timesec_usec(abstimeout, timeout_us);

	return 0;
}

/**
 * ds_make_timeout_ms - utility function from making absolute timeout from
 *			relative millisecond timeout
 * @abstimeout: absolute time of timeout
 * @timeout_ms: relative timeout in milliseconds
 */
int ds_make_timeout_ms(struct ds_timespec *abstimeout, unsigned int timeout_ms)
{
	return ds_make_timeout_us(abstimeout, timeout_ms * 1000);
}
