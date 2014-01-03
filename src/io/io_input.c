/*
 * Modular input subsystem
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

#include <stdlib.h>

#include "io.h"

/**
 * io_input_process - process currently available input
 */
int io_input_process(struct io_input *input, int *error)
{
	enum io_parser_ret eret;
	bool end_of_input;
	int ret;

	*error = 0;
	ret = io_input_read(input, error); /* non-blocking */
	if (ret == 0) {
		/* no new input */
		return 0;
	}

	/* End of input, let parser handle end of file */
	end_of_input = (ret < 0);

	/* might modify buffer or leave as is if not enough input yet */
	eret = io_parser_parse(input->parser, &input->inbuf, end_of_input);

	if (end_of_input && ds_append_buffer_length(&input->inbuf) > 0) {
		/*
		 * At end of data, clear buffer.
		 * Parser had chance to handle the data.
		 */
		ds_append_buffer_free(&input->inbuf);
		ds_append_buffer_init(&input->inbuf);
	}

	switch (eret) {
	case IO_PARSER_RET_CONTINUE:
		if (end_of_input)
			return -1;
		return 0;
	case IO_PARSER_RET_QUEUE_FULL:
		return 1;
	case IO_PARSER_RET_ERROR:
	default:
		return -1;
	}
}

/**
 * io_input_process_loop - processing loop for input
 */
void io_input_process_loop(struct io_input *input)
{
	int error, ret;

	while (io_input_wait(input) == IO_INPUT_WAIT_NEW) {
		error = 0;
		ret = io_input_process(input, &error);
		if (ret < 0)
			break;
		if (ret > 0) {
			/*
			 * Input queue was full. Wait until there is room for
			 * more input values.
			 */
			if (!io_parser_wait_queue(input->parser))
				break;
		}
	}
}

/**
 * io_input_read - wrapper around input->ops->read, reads data from input to
 *		   input->inbuf.
 */
int io_input_read(struct io_input *input, int *error)
{
	if (input->ops->read)
		return input->ops->read(input, error);

	return -1;
}

/**
 * io_input_wait - wrapper around input->ops->wait, waits data to become
 *		   available at input
 */
enum io_input_wait_ret io_input_wait(struct io_input *input)
{
	if (input->ops->wait)
		return input->ops->wait(input);

	return IO_INPUT_WAIT_ERROR;
}

/**
 * io_input_stop_wait - wrapper around input->ops->stop_wait, interrupts waiting
 *			of input in remote thread
 */
bool io_input_stop_wait(struct io_input *input)
{
	if (input->ops->stop_wait)
		return input->ops->stop_wait(input);

	return false;
}

/**
 * io_input_destroy - wrapper around input->ops->destroy, frees internal
 *		      io_input structures
 */
bool io_input_destroy(struct io_input *input)
{
	if (input->ops->destroy)
		return input->ops->destroy(input);

	ds_append_buffer_free(&input->inbuf);
	free(input);

	return true;
}

/**
 * io_input_reopen - wrapper around input->ops->reopen, reopens input source
 */
bool io_input_reopen(struct io_input *input)
{
	if (input->ops->reopen)
		return input->ops->reopen(input);

	return false;
}

