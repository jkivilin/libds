/*
 * Simple IO library with stackable parsers (developed as part of a ECG-monitor
 * student project)
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

#ifndef __LIBIO__IO_H__
#define __LIBIO__IO_H__

#include <stddef.h>
#include "ds.h"


/*****************************************************************************
 * Helper functions
 *****************************************************************************/

/**
 * io_set_fd_nonblocking - set unix file/socket descriptor to non-blocking mode
 * @fd: file descriptor to modify
 *
 * Returns true if successfull
 */
extern bool io_set_fd_nonblocking(int fd);

/**
 * io_sleep_to - sleep to absolute timeout
 * @abstime: time in future to sleep to
 */
extern void io_sleep_to(struct ds_timespec *abstime);

/**
 * io_usleep - sleep for @usec microseconds
 */
extern void io_usleep(unsigned int usec);

/**
 * (internal) io_set_latest_error - set error string for currently happened
 *				    error
 * @fmt: snprintf format string
 * @...: format arguments
 */
extern void io_set_latest_error(const char *fmt, ...);

/**
 * io_get_latest_error - get error string of latest happened libio error
 *
 * Return pointer to static string buffer, not thread-safe!
 */
extern const char *io_get_latest_error(void);


/*****************************************************************************
 * Modular nested parser subsystem
 *****************************************************************************/

/*
 * Nested parsers allow different parsers to be stacked over each. For example
 * this system allows (HTTP(GZIP(TXT))) encapsulated data handling as separate
 * HTTP, GZIP and TXT parsers.
 */

struct io_parser;

enum io_parser_ret {
	IO_PARSER_RET_CONTINUE = 0,
	IO_PARSER_RET_QUEUE_FULL,
	IO_PARSER_RET_ERROR,
};

struct io_parser_ops {
	enum io_parser_ret (*parse)(struct io_parser *parser,
				    struct ds_append_buffer *buffer,
				    bool final);
	bool (*wait_queue)(struct io_parser *parser);
	bool (*destroy)(struct io_parser *parser);
	bool (*reset)(struct io_parser *parser);
};

struct io_parser {
	const struct io_parser_ops *ops;
};

/**
 * io_parser_init - initialize io_parser structure
 */
static inline void io_parser_init(struct io_parser *parser,
				  const struct io_parser_ops *ops)
{
	parser->ops = ops;
}

/**
 * io_parser_parse - parse data in received input buffer
 * @parser: parser to use
 * @buffer: input stream buffer
 * @final: if true, input buffer contains last set of data and should be
 *         prosessed fully by parser
 *
 * Parser may partially or fully, or may not process data in input buffer.
 * Parser leaves unprocessed data in @buffer for next call to io_parser_parse.
 * Function will not be called if previous io_parser_parse() call was made with
 * @final == true.
 *
 * Returns IO_PARSER_RET_ERROR on hard parser error (parser cannot continue
 * parsing this input stream anymore. IO_PARSER_RET_QUEUE_FULL, if parser queue
 * is full and IO_PARSER_RET_CONTINUE if parser is ready to continue parsing.
 */
extern enum io_parser_ret io_parser_parse(struct io_parser *parser,
					  struct ds_append_buffer *buffer,
					  bool final);

/**
 * io_parser_wait_queue - waits internal queue to become free
 */
extern bool io_parser_wait_queue(struct io_parser *parser);

/**
 * io_parser_destroy - free internal parser structures
 */
extern bool io_parser_destroy(struct io_parser *parser);

/**
 * io_parser_reset - resets parser and internal structures
 */
extern bool io_parser_reset(struct io_parser *parser);


/*****************************************************************************
 * Text parser
 *****************************************************************************/
/*
 * Final parser for text-format ECG data (top of parser stack, input from
 * bottom)
 */

/**
 * io_new_text_parser - allocate and initialize new EGC text parser
 */
extern struct io_parser *io_new_text_parser(void);


/*****************************************************************************
 * gzip file parser/decompressor
 *****************************************************************************/

/**
 * io_new_gz_parser - allocate and initialize new GZ parser
 */
extern struct io_parser *io_new_gz_parser(struct io_parser *child);


/*****************************************************************************
 * Modular input subsystem
 *****************************************************************************/
struct io_input;

enum io_input_wait_ret {
	IO_INPUT_WAIT_NEW = 0,
	IO_INPUT_WAIT_STOP,
	IO_INPUT_WAIT_ERROR
};

/**
 * io_input_ops - input functions provided by module
 * @wait: waits for input from source (return false when stopped by stop_wait)
 * @read: reads input from source (must be non-blocking)
 * @stop_wait: stops wait that was previously issued (return false on error)
 */
struct io_input_ops {
	int (*read)(struct io_input *input, int *error);
	enum io_input_wait_ret (*wait)(struct io_input *input);
	bool (*stop_wait)(struct io_input *input);
	bool (*destroy)(struct io_input *input);
	bool (*reopen)(struct io_input *input);
};

struct io_input {
	const struct io_input_ops *ops;
	struct io_parser *parser;
	struct ds_append_buffer inbuf;
};

/**
 * io_input_init - initialize io_input structure
 * @input: input structure to initialize
 * @ops: operators for current input module
 * @parser: bottom of parser stack to use
 */
static inline void io_input_init(struct io_input *input,
				 const struct io_input_ops *ops,
				 struct io_parser *parser)
{
	ds_append_buffer_init(&input->inbuf);
	input->ops = ops;
	input->parser = parser;
}

/**
 * io_input_init - free internal io_input structures
 */
static inline void io_input_free(struct io_input *input)
{
	ds_append_buffer_free(&input->inbuf);
}

/**
 * io_input_process - process currently available input
 */
extern int io_input_process(struct io_input *input, int *error);

/**
 * io_input_process_loop - processing loop for input
 */
extern void io_input_process_loop(struct io_input *input);

/**
 * io_input_read - wrapper around input->ops->read, reads data from input to
 *		   input->inbuf.
 */
extern int io_input_read(struct io_input *input, int *error);

/**
 * io_input_wait - wrapper around input->ops->wait, waits data to become
 *		   available at input
 */
extern enum io_input_wait_ret io_input_wait(struct io_input *input);

/**
 * io_input_stop_wait - wrapper around input->ops->stop_wait, interrupts
 *			waiting of input in remote thread
 */
extern bool io_input_stop_wait(struct io_input *input);

/**
 * io_input_destroy - wrapper around input->ops->destroy, frees internal
 *		      io_input structures
 */
extern bool io_input_destroy(struct io_input *input);

/**
 * io_input_reopen - wrapper around input->ops->reopen, reopens input source
 */
extern bool io_input_reopen(struct io_input *input);


/*****************************************************************************
 * Generic file descriptor input module
 *****************************************************************************/

typedef int (*io_generic_fd_open)(struct io_input *io, void *priv);
typedef bool (*io_generic_fd_close)(struct io_input *io, void *priv);

/**
 * io_new_generic_fd_input - allocate and initialize file descriptor input
 *			     module
 * @parser: bottom of parser stack to use
 * @open_func: function for opening/reopening file descriptor
 * @close_func: function for closing file descriptor
 * @priv: private data for open_func
 */
extern struct io_input *io_new_generic_fd_input(struct io_parser *parser,
						io_generic_fd_open open_func,
						io_generic_fd_close close_func,
						void *priv);


/*****************************************************************************
 * File input module
 *****************************************************************************/
/*
 * Simple file input module
 */

/**
 * io_new_file_input - allocate and initialize file input module
 * @parser: bottom of parser stack to use
 * @filename: file to open for input
 */
extern struct io_input *io_new_file_input(struct io_parser *parser,
					  const char *filename);


/*****************************************************************************
 * External input module
 *****************************************************************************/

/*
 * Input module for external input, for example from Bluetooth/device-driver in
 * ObjC.
 * New data is pushed from ObjC/driver using io_external_input_push_data(). For
 * examples, see test-unit source-code.
 */

/**
 * io_new_external_input - allocate and initialize external input module
 * @parser: bottom of parser stack to use
 */
extern struct io_input *io_new_external_input(struct io_parser *parser);

/**
 * io_external_input_push_data - push new data to input system
 * @ext_input: input structure allocated with io_new_external_input()
 * @buf: buffer with new data
 * @buflen: length of buffer in bytes
 *
 * Returns number of bytes pushed (returns < buflen when out of memory).
 */
extern int io_external_input_push_data(struct io_input *ext_input, void *buf,
				       unsigned int buflen);


/*****************************************************************************
 * File saving support
 *****************************************************************************/
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
extern bool io_save_txt_file(const char *filename, float *values,
			     unsigned int num_values);

/**
 * io_save_gz_txt_file - save @values to file with name @filename, using text
 *			 formating and gzip compression.
 * @filename: filename to use
 * @values: data points to save (ecg data, 4ms intervals, 250pps)
 * @num_values: number of values
 *
 * Text format is same as with io_save_txt_file()
 */
extern bool io_save_gz_txt_file(const char *filename, float *values,
				unsigned int num_values);


/*****************************************************************************
 * IO library main functions
 *****************************************************************************/
/**
 * io_open_txt_file_input - open file with name @filename for global input
 */
extern void io_open_txt_file_input(const char *filename);

/**
 * io_open_txt_external_input - open external text input for global input
 */
extern void io_open_txt_external_input(void);

/**
 * io_push_external_input - push data to global input that has been opened with
 *			    io_open_external_input()
 * @buf: buffer with new data
 * @buflen: length of buffer in bytes
 *
 * Returns number of bytes pushed (returns < buflen when out of memory).
 */
extern int io_push_external_input(void *buf, unsigned int buflen);

/**
 * io_close_main_input - close previously global input
 */
extern void io_close_main_input(void);


/*****************************************************************************
 * ECG data input, main IO queue
 *****************************************************************************/
/**
 * io_main_queue_push_4ms_interval_value - add @data_value to global ECG input
 * 					   data queue
 */
extern bool io_main_queue_push_4ms_interval_value(float data_value);

/**
 * io_main_queue_get_next_values - process next batch of input data to ECG data
 *				   values
 * @values: values buffer to fill with new values
 * @num_values: elements in @values buffer
 */
extern bool io_main_queue_get_next_values(float *values,
					  unsigned int num_values);

/**
 * io_main_get_next_data_line - get data line buffer for sending ECG data over
 *                              network.
 * @line_buffer: buffer to write data line to
 * @buflen: length of buffer
 *
 * Returns number of bytes written to buffer.
 */
extern unsigned int io_main_get_next_data_line(char *line_buffer,
					       unsigned int buflen);

#endif /*__LIBIO__IO_H__*/
