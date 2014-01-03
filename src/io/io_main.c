/*
 * IO library main functions
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <math.h>

#include "io.h"

static struct io_main {
	struct io_input *input;
	struct ds_append_buffer input_values_buffer;
	struct ds_timespec next_time;
} io_main;

static pthread_mutex_t main_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * __io_close_main_input - close previously global input, lockless
 */
static void __io_close_main_input(void)
{
	if (io_main.input) {
		io_input_destroy(io_main.input);
		ds_append_buffer_free(&io_main.input_values_buffer);

		memset(&io_main, 0, sizeof(io_main));
	}
}

/**
 * io_close_main_input - close previously global input
 */
void io_close_main_input(void)
{
	pthread_mutex_lock(&main_lock);
	__io_close_main_input();
	pthread_mutex_unlock(&main_lock);
}

/**
 * io_set_main_input - set new global input
 */
static void io_set_main_input(struct io_input *input)
{
	pthread_mutex_lock(&main_lock);

	/* Close currently open input */
	__io_close_main_input();

	/* Create new main input, TXT parser, file input */
	io_main.input = input;

	/*
	 * Use append buffer for qeueuing as it's more effencient at storing
	 * large data sets than ds_queue
	 */
	ds_append_buffer_init(&io_main.input_values_buffer);

	/* Clear timer */
	memset(&io_main.next_time, 0, sizeof(io_main.next_time));

	pthread_mutex_unlock(&main_lock);
}

/**
 * io_open_txt_file_input - open file with name @filename for global input
 *
 * Basic file handler, ECG data in ASCII file
 */
void io_open_txt_file_input(const char *filename)
{
	/* Create new main input, TXT parser, file input */
	io_set_main_input(io_new_file_input(io_new_gz_parser(
		io_new_text_parser()), filename));
}

/**
 * io_open_txt_external_input - open external text input for global input
 */
void io_open_txt_external_input(void)
{
	/* Create new external input with TXT parser */
	io_set_main_input(io_new_external_input(io_new_text_parser()));
}

/**
 * io_push_external_input - push data to global input that has been opened with
 *			    io_open_external_input()
 * @buf: buffer with new data
 * @buflen: length of buffer in bytes
 *
 * Returns number of bytes pushed (returns < buflen when out of memory).
 */
int io_push_external_input(void *buf, unsigned int buflen)
{
	int ret;

	pthread_mutex_lock(&main_lock);
	ret = io_external_input_push_data(io_main.input, buf, buflen);
	pthread_mutex_unlock(&main_lock);

	return ret;
}

/**
 * io_main_queue_push_4ms_interval_value - add @data_value to global ECG input
 *					   data queue
 */
bool io_main_queue_push_4ms_interval_value(float data_value)
{
	/*
	 * TODO: Find and fix the real bug... our iPhone part is having strange
	 * performance problem with too accurate floating-point values.
	 */
	data_value = roundf(data_value * 100.0f) / 100;

	ds_append_buffer_append(&io_main.input_values_buffer, &data_value,
				sizeof(data_value));

	return true;
}

/**
 * io_main_queue_get_next_values - process next batch of input data to ECG data
 *				   values
 * @values: values buffer to fill with new values
 * @num_values: elements in @values buffer
 */
bool io_main_queue_get_next_values(float *values, unsigned int num_values)
{
	int ret, error, i;
	bool retval;

	pthread_mutex_lock(&main_lock);

	if (!io_main.input) {
		/* In error case, clear input values array */
		for (i = 0; i < num_values; i++)
			values[i] = 0.0f;

		retval = false;
		goto out;
	}

	/*
	 * For now, do IO in caller thread context. This may be moved to remote
	 * IO thread in future.
	 */
	while (ds_append_buffer_length(&io_main.input_values_buffer) <
						num_values * sizeof(float)) {
		bool reopen = false;

		switch (io_input_wait(io_main.input)) {
		case IO_INPUT_WAIT_NEW:
			ret = io_input_process(io_main.input, &error);
			if (ret < 0)
				reopen = true;
			break;
		case IO_INPUT_WAIT_ERROR:
			reopen = true;
			break;
		case IO_INPUT_WAIT_STOP:
		default:
			retval = false;
			goto out;
		}

		if (reopen && io_input_reopen(io_main.input) < 0) {
			retval = false;
			goto out;
		}
	}

	/* Copy data from values queue to values array. */
	ds_append_buffer_copy(&io_main.input_values_buffer, 0, values,
			      num_values * sizeof(float));

	/* Clear copied data from values queue. */
	ds_append_buffer_move_head(&io_main.input_values_buffer,
				   num_values * sizeof(float));

	if (io_main.next_time.tv_sec != 0 || io_main.next_time.tv_nsec != 0) {
		/*
		 * Set up sleep. We receive values at average 4ms intervals so
		 * sleep to (last_time + (4ms * num_values)).
		 */
		ds_add_timesec_usec(&io_main.next_time, 4000 * num_values);
		
		/*
		 * Sleep until enough time is passed since last read. This is
		 * to fill time between reads, so that libio does not return
		 * values at higher than 4ms interval rate.
		 * 
		 * Note: This might not sleep at all, as sleep is to absolute
		 * time. That time might be in past.
		 */
		io_sleep_to(&io_main.next_time);
	} else {
		/* On first run, just initialize timer. */
		ds_get_curr_timespec(&io_main.next_time);
	}

	retval = true;
out:
	pthread_mutex_unlock(&main_lock);

	return retval;
}

/**
 * io_main_get_next_data_line - get data line buffer for sending ECG data over
 *				network.
 * @line_buffer: buffer to write data line to
 * @buflen: length of buffer
 *
 * Returns number of bytes written to buffer.
 */
unsigned int io_main_get_next_data_line(char *line_buffer, unsigned int buflen)
{
	float value = 0;
	int slen;

	/* get next data value to send */
	io_main_queue_get_next_values(&value, 1);

	slen = snprintf(line_buffer, buflen, "%.02f\n", value);
	if (slen < 0)
		return 0;

	return slen;
}
