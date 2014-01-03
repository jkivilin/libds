/*
 * ECG text-file parser
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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "io.h"

#define TEXT_PARSER_MAX_LINE_LEN 64

enum text_state {
	CHECK_FIRST_LINES = 0,
	CHECK_FIRST_LINES_IS_DELTAENCODED,
	DETECT_FILE_TYPE,
	HANDLE_DATE_INTERVAL_FILE,
	HANDLE_FLOAT_INTERVAL_FILE,
	HANDLE_4MS_FIXED_INTERVAL_FILE,
};

struct text_parser_priv {
	struct io_parser parser;

	enum text_state state;
	bool first_read;
	bool delta_encoded;

	/*
	 * for interpolating variable input interval to 4ms.
	 *
	 * timekeeping requires 'double' because start_time might be large.
	 */
	double start_time, cur_time, prev_time;
	unsigned long long int add_time_ms;
	float cur_value,  prev_value;
};

static inline struct text_parser_priv *
				text_parser_priv(struct io_parser *parser)
{
	return ds_container_of(parser, struct text_parser_priv, parser);
}

static float interpolate(float prev_time, float next_time, float prev_value,
			 float next_value, float cur_time)
{
	float value_len = (next_value - prev_value);
	float time_len = (next_time - prev_time);
	float cur_prev_len = (cur_time - prev_time);

	return (cur_prev_len * value_len / time_len) + prev_value;
}

static void adjust_interval(struct text_parser_priv *priv, double second,
			    float value)
{
	float cur_value;

	if (!priv->first_read) {		
		priv->prev_value = value;
		priv->prev_time = second;
		priv->start_time = second;
		priv->add_time_ms = 0;
		priv->cur_time = priv->start_time + priv->add_time_ms / 1000.0;

		priv->first_read = true;
		return;
	}

	if (priv->delta_encoded) {
		second = priv->prev_time + second;
		value = priv->prev_value + value;
	}

	while (priv->cur_time <= second + 0.0001) {
		cur_value = interpolate(priv->prev_time, second,
					priv->prev_value, value,
					priv->cur_time);

		io_main_queue_push_4ms_interval_value(cur_value);

		priv->add_time_ms += 4;
		priv->cur_time = priv->start_time + priv->add_time_ms / 1000.0;
	}

	priv->prev_time = second;
	priv->prev_value = value;
}

static enum io_parser_ret text_parser_handle_line(struct text_parser_priv *priv,
						  char *line, bool final)
{
	float value;
	double second;
	int minute;
	int num = 0;
	bool first_line = false;

new_state:
	switch (priv->state) {
	case CHECK_FIRST_LINES:
		priv->delta_encoded = false;
		/* fall-through */
	case CHECK_FIRST_LINES_IS_DELTAENCODED:
		first_line = true;
		priv->state = DETECT_FILE_TYPE;
		priv->first_read = false;
		/* fall-through */
	case DETECT_FILE_TYPE:
		num = sscanf(line, "%d:%lf %f", &minute, &second, &value);
		if (num == 3) {
			priv->state = HANDLE_DATE_INTERVAL_FILE;
			goto new_state;
		}

		num = sscanf(line, "%lf %f", &second, &value);
		if (num == 2) {
			priv->state = HANDLE_FLOAT_INTERVAL_FILE;
			goto new_state;
		}

		if (num == 1) {
			priv->state = HANDLE_4MS_FIXED_INTERVAL_FILE;
			goto new_state;
		}

		/* Ignore invalid first lines */
		if (first_line) {
			/* Check for delta-encoding marker */
			if (strcmp(line, "#deltaenc") == 0) {
				priv->state = CHECK_FIRST_LINES_IS_DELTAENCODED;
				priv->delta_encoded = true;
			} else
				priv->state = CHECK_FIRST_LINES;

			return IO_PARSER_RET_CONTINUE;
		}

		return IO_PARSER_RET_ERROR;

	case HANDLE_DATE_INTERVAL_FILE:
		num = sscanf(line, "%d:%lf %f", &minute, &second, &value);
		if (num == 3) {
			adjust_interval(priv, second + 60.0 * minute, value);
		} else {
			/* 
			 * restarted reading file from begining, need to reset.
			 */
			priv->state = CHECK_FIRST_LINES;
			goto new_state;
		}

		break;

	case HANDLE_FLOAT_INTERVAL_FILE:
		num = sscanf(line, "%lf %f", &second, &value);
		if (num == 2) {
			adjust_interval(priv, second, value);
		} else {
			/*
			 * restarted reading file from begining, need to reset.
			 */
			priv->state = CHECK_FIRST_LINES;
			goto new_state;
		}

		break;

	case HANDLE_4MS_FIXED_INTERVAL_FILE:
		num = sscanf(line, "%f", &value);
		if (num == 1) {
			if (!priv->first_read) {
				priv->prev_value = 0.0f;
				priv->first_read = true;
			}

			if (priv->delta_encoded) {
				value += priv->prev_value;
				priv->prev_value = value;
			}

			io_main_queue_push_4ms_interval_value(value);
		} else {
			/*
			 * restarted reading file from begining, need to reset.
			 */
			priv->state = CHECK_FIRST_LINES;
			goto new_state;
		}

		break;
	}

	return IO_PARSER_RET_CONTINUE;
}

static enum io_parser_ret text_parser_parse(struct io_parser *parser,
					    struct ds_append_buffer *buffer,
					    bool final)
{
	struct text_parser_priv *priv = text_parser_priv(parser);
	struct ds_append_buffer_iterator iter;
	char buf[TEXT_PARSER_MAX_LINE_LEN];
	enum io_parser_ret eret;
	unsigned int llen, clen;

restart:
	ds_append_buffer_for_each(&iter, buffer) {
		/* Find end of line */
		if (ds_append_buffer_iterator_byte(&iter) == '\n') {
			llen = ds_append_buffer_iterator_pos(&iter) + 1;
			clen = llen >= sizeof(buf) ? sizeof(buf) : llen;

			/* copy line to temporary buffer */
			ds_append_buffer_copy(buffer, 0, buf, clen);

			/* null terminate string */
			buf[clen - 1] = 0;

			/* move buffer head forward */
			ds_append_buffer_move_head(buffer, llen);

			/* handle new line */
			eret = text_parser_handle_line(priv, buf, false);
			if (eret != IO_PARSER_RET_CONTINUE)
				return eret;

			goto restart;
		}
	}

	if (final) {
		llen = ds_append_buffer_length(buffer) + 1;
		clen = llen >= sizeof(buf) ? sizeof(buf) : llen;

		/* copy line to temporary buffer */
		ds_append_buffer_copy(buffer, 0, buf, clen - 1);
		buf[clen - 1] = '\0';

		/* handle new line */
		return text_parser_handle_line(priv, buf, final);
	}

	return IO_PARSER_RET_CONTINUE;
}

static bool text_parser_wait_queue(struct io_parser *parser)
{
	return true;
}

static bool text_parser_destroy(struct io_parser *parser)
{
	free(parser);
	return true;
}

static bool text_parser_reset(struct io_parser *parser)
{
	struct text_parser_priv *priv = text_parser_priv(parser);

	priv->state = CHECK_FIRST_LINES;
	priv->first_read = false;

	return true;
}

static const struct io_parser_ops text_parser_ops = {
	.parse = text_parser_parse,
	.wait_queue = text_parser_wait_queue,
	.destroy = text_parser_destroy,
	.reset = text_parser_reset,
};

/**
 * io_new_text_parser - allocate and initialize new EGC text parser
 */
struct io_parser *io_new_text_parser(void)
{
	struct text_parser_priv *priv;

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		io_set_latest_error("%s():%d: calloc failed (errno: %d)",
				    __func__, __LINE__, errno);
		return NULL;
	}

	io_parser_init(&priv->parser, &text_parser_ops);
	io_parser_reset(&priv->parser);

	return &priv->parser;
}
