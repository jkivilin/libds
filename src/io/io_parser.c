/*
 * Modular nested parser subsystem
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
 *
 * Nested parsers allow different parsers to be stacked over each. For example
 * this system allows (HTTP(GZIP(TXT))) encapsulated data handling as separate
 * HTTP, GZIP and TXT parsers.
 */

#include <stdlib.h>

#include "io.h"

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
enum io_parser_ret io_parser_parse(struct io_parser *parser,
				   struct ds_append_buffer *buffer,
				   bool final)
{
	if (parser->ops->parse)
		return parser->ops->parse(parser, buffer, final);

	return IO_PARSER_RET_ERROR;
}

/**
 * io_parser_wait_queue - waits internal queue to become free
 */
bool io_parser_wait_queue(struct io_parser *parser)
{
	if (parser->ops->parse)
		return parser->ops->wait_queue(parser);

	return false;
}

/**
 * io_parser_destroy - free internal parser structures
 */
bool io_parser_destroy(struct io_parser *parser)
{
	if (parser->ops->destroy)
		return parser->ops->destroy(parser);

	free(parser);
	return true;
}

/**
 * io_parser_reset - resets parser and internal structures
 */
bool io_parser_reset(struct io_parser *parser)
{
	if (parser->ops->reset)
		return parser->ops->reset(parser);

	return true;
}
