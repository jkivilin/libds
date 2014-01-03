/*
 * gzip parser
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

#include "priv_zlib.h"
#include "io.h"

#define GZ_MAGIC_ID1 0x1f
#define GZ_MAGIC_ID2 0x8b
#define GZ_MAGIC_CM 0x08

#define GZIP_HEADER_LEN 10

enum gz_flags {
	GZIP_FLAG_FTEXT = (1 << 0),
	GZIP_FLAG_FHCRC = (1 << 1),
	GZIP_FLAG_FEXTRA = (1 << 2),
	GZIP_FLAG_FNAME = (1 << 3),
	GZIP_FLAG_FCOMMENT = (1 << 4),
};

enum gz_state {
	CHECK_MAGIC = 0,
	PARSE_GZIP_HEADER,
	PARSE_GZIP_FEXTRA,
	PARSE_GZIP_FNAME,
	PARSE_GZIP_FCOMMENT,
	PARSE_GZIP_FHCRC,
	DO_PASSTHROUGH,
	DO_DECOMPRESSION,
	DONE,
};

struct gz_parser_priv {
	struct io_parser parser;
	struct io_parser *child;

	enum gz_state state;
	unsigned int gz_flags;

	bool zlib_initialized;
	struct ds_append_buffer decompr_buf;
	z_stream zstream;
	unsigned char z_buf[256];
	unsigned int z_buf_bytes;
};

static inline struct gz_parser_priv *gz_parser_priv(struct io_parser *parser)
{
	return ds_container_of(parser, struct gz_parser_priv, parser);
}

static void *gz_zalloc(void *o, unsigned int items, unsigned int size)
{
	return calloc(items, size);
}

static void gz_zfree(void *o, void *mem)
{
	free(mem);
}

static void gz_close_zlib(struct io_parser *parser)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);

	if (!priv->zlib_initialized)
		return;

	inflateEnd(&priv->zstream);
	ds_append_buffer_free(&priv->decompr_buf);

	priv->zlib_initialized = false;
	priv->z_buf_bytes = 0;
}

static void gz_initialize_zlib(struct io_parser *parser)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);

	if (priv->zlib_initialized)
		gz_close_zlib(parser);

	ds_append_buffer_init(&priv->decompr_buf);
	memset(&priv->zstream, 0, sizeof(priv->zstream));

	priv->zstream.zalloc = gz_zalloc;
	priv->zstream.zfree =gz_zfree;
	priv->zstream.next_in = Z_NULL;
	priv->zstream.avail_in = 0;
	inflateInit2(&priv->zstream, -15);

	priv->zlib_initialized = true;
	priv->z_buf_bytes = 0;
}

static bool gz_skip_null_term_string(struct ds_append_buffer *buffer)
{
	struct ds_append_buffer_iterator iter;
	unsigned int len;
	bool found;

	/* Try find '\0' */
	found = false;
	ds_append_buffer_for_each(&iter, buffer) {
		if (ds_append_buffer_iterator_byte(&iter) == '\0') {
			found = true;
			break;
		}
	}

	/* found end of string? */
	if (found) {
		/* skip end of null-term string */
		len = ds_append_buffer_iterator_pos(&iter) + 1;
	} else {
		/*
		 * discard handled bytes, and continue with string handling
		 * when receiving more data.
		 */
		len = ds_append_buffer_length(buffer);
	}

	ds_append_buffer_move_head(buffer, len);

	return found;
}

static int gz_decompress(struct io_parser *parser,
			 struct ds_append_buffer *buffer, bool final,
			 int *outret)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);
	z_stream *zinf = &priv->zstream;
	unsigned int out_bytes, wbuflen = 0;
	void *write_buf;
	int ret;

	/* Check if data in z_buf has been processed */
	if (zinf->avail_in == 0) {
		/* read in new bytes */
		priv->z_buf_bytes = ds_append_buffer_copy(buffer, 0,
							  priv->z_buf,
							  sizeof(priv->z_buf));

		zinf->next_in = priv->z_buf;
		zinf->avail_in = priv->z_buf_bytes;

		if (priv->z_buf_bytes == 0) {
			/* no more input */
			return -1;
		}

		/* move buffer head forward */
		ds_append_buffer_move_head(buffer, priv->z_buf_bytes);
	}

	/* Avoid memory copies, inflate() directly to append_buffer... */
	write_buf = ds_append_buffer_get_write_buffer(&priv->decompr_buf,
						      &wbuflen);
	if (!write_buf) {
		/* out of mem */
		return -1;
	}

	zinf->next_out = write_buf;
	zinf->avail_out = wbuflen;

	/* TODO: test-case for final and Z_FINISH */
	ret = inflate(zinf, /*final ? Z_FINISH :*/ Z_SYNC_FLUSH);

	if (ret == Z_OK || ret == Z_STREAM_END) {
		if (ret == Z_STREAM_END) {
			priv->state = DONE;
			final = true;
		}

		out_bytes = wbuflen - zinf->avail_out;

		/* append new write buffer at end of appendable buffer */
		ds_append_buffer_finish_write_buffer(&priv->decompr_buf,
						     write_buf, out_bytes);

		/* pass bytes to child parser */
		ret = io_parser_parse(priv->child, &priv->decompr_buf, final);
		if (ret != IO_PARSER_RET_CONTINUE || priv->state == DONE) {
			*outret = ret;
			return 1;
		}
	} else {
		/* free unused write buffer */
		ds_append_buffer_finish_write_buffer(&priv->decompr_buf,
						     write_buf, 0);

		if (ret == Z_BUF_ERROR) {
			/* 
			 * non-fatal error, means: not enough out_buf when
			 * Z_FINISH is used.
			 */
		} else {
			/* fatal error */
			io_set_latest_error("%s():%d: inflate failed "
					    "(errno: %d, err-msg: %s)",
					    __func__, __LINE__, ret, zinf->msg);
			*outret = IO_PARSER_RET_ERROR;
			return 1;
		}
	}

	return 0;
}

static enum io_parser_ret gz_parser_parse(struct io_parser *parser,
					  struct ds_append_buffer *buffer,
					  bool final)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);
	unsigned char header[GZIP_HEADER_LEN];
	unsigned int len;
	int ret, loopret;

new_state:
	switch (priv->state) {
	/* First, check file for gz magic */
	case CHECK_MAGIC:
		priv->gz_flags = 0;

		if (ds_append_buffer_length(buffer) < 3)
			return IO_PARSER_RET_CONTINUE;

		if (ds_append_buffer_copy(buffer, 0, header, 3) < 3)
			return IO_PARSER_RET_CONTINUE;

		if (header[0] == GZ_MAGIC_ID1 && header[1] == GZ_MAGIC_ID2 &&
		    header[2] == GZ_MAGIC_CM) {
			/*
			 * Parse gzip-header before doing decompression.
			 */
			priv->state = PARSE_GZIP_HEADER;
		} else {
			/* 
			 * File not compressed, do pass data directly to next
			 * parser.
			 */
			priv->state = DO_PASSTHROUGH;
			goto new_state;
		}

		/* Passthrough */

	/* Parse GZIP main header */
	case PARSE_GZIP_HEADER:
		/* main header has 10 bytes */
		if (ds_append_buffer_length(buffer) < GZIP_HEADER_LEN)
			return IO_PARSER_RET_CONTINUE;

		if (ds_append_buffer_copy(buffer, 0, header, GZIP_HEADER_LEN) <
								GZIP_HEADER_LEN)
			return IO_PARSER_RET_CONTINUE;

		ds_append_buffer_move_head(buffer, GZIP_HEADER_LEN);

		priv->gz_flags = header[3];
		priv->state = PARSE_GZIP_FEXTRA;

		/* Passthrough */
	case PARSE_GZIP_FEXTRA:
		/* Handle extra header if gzip-flag say that file contains it */
		if (priv->gz_flags & GZIP_FLAG_FEXTRA) {
			if (ds_append_buffer_length(buffer) < 2)
				return IO_PARSER_RET_CONTINUE;

			if (ds_append_buffer_copy(buffer, 0, header, 2) < 2)
				return IO_PARSER_RET_CONTINUE;

			len = header[0];
			len += (unsigned int)header[1] << 8;

			if (ds_append_buffer_length(buffer) < 2 + len)
				return IO_PARSER_RET_CONTINUE;

			/* skip extra header */
			ds_append_buffer_move_head(buffer, 2 + len);
		}

		priv->state = PARSE_GZIP_FNAME;

		/* Passthrough */
	case PARSE_GZIP_FNAME:
		/* 
		 * Handle filename header if gzip-flag say that file contains
		 * it.
		 */
		if (priv->gz_flags & GZIP_FLAG_FNAME) {
			if (!gz_skip_null_term_string(buffer))
				return IO_PARSER_RET_CONTINUE;
		}

		priv->state = PARSE_GZIP_FCOMMENT;

		/* Passthrough */
	case PARSE_GZIP_FCOMMENT:
		/*
		 * Handle comment header if gzip-flag say that file contains
		 * it.
		 */
		if (priv->gz_flags & GZIP_FLAG_FCOMMENT) {
			if (!gz_skip_null_term_string(buffer))
				return IO_PARSER_RET_CONTINUE;
		}

		priv->state = PARSE_GZIP_FHCRC;

		/* Passthrough */
	case PARSE_GZIP_FHCRC:
		/*
		 * Handle header-checksum if gzip-flag say that file contains
		 * it.
		 */
		if (priv->gz_flags & PARSE_GZIP_FHCRC) {
			if (ds_append_buffer_length(buffer) < 2)
				return IO_PARSER_RET_CONTINUE;

			/* skip header crc */
			ds_append_buffer_move_head(buffer, 2);
		}

		ds_append_buffer_copy(buffer, 0, header, 2);

		/* Header parsed, continue to decompression */
		gz_initialize_zlib(parser);
		priv->state = DO_DECOMPRESSION;

		goto new_state;

	/* Just pass data buffer to underlying parser */
	case DO_PASSTHROUGH:
	default:
		return io_parser_parse(priv->child, buffer, final);

	/*
	 * Pass compressed data from input buffer to zlib, and decompressed
	 * data from zlib to new buffer and pass new buffer to next parser.
	 */
	case DO_DECOMPRESSION:
		/*
		* Do gzip decompression.
		*/
		do {
			loopret = gz_decompress(parser, buffer, final, &ret);
			if (loopret == 1)
				return ret;
		} while (loopret == 0);

		/* If final flag is set, let child parser know/flush */
		if (final)
			return io_parser_parse(priv->child, &priv->decompr_buf,
					       true);

		return IO_PARSER_RET_CONTINUE;

	/* Decompression done, ignore trailing bytes. */
	case DONE:
		ds_append_buffer_move_head(buffer,
					   ds_append_buffer_length(buffer));
		return IO_PARSER_RET_CONTINUE;
	}
}

static bool gz_parser_wait_queue(struct io_parser *parser)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);

	switch (priv->state) {
	case DO_PASSTHROUGH:
		return io_parser_wait_queue(priv->child);
	case DO_DECOMPRESSION:
		/* TODO: some work needed here? */
		return true;
	default:
		return true;
	}
}

static bool gz_parser_destroy(struct io_parser *parser)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);

	gz_close_zlib(parser);
	io_parser_destroy(priv->child);
	free(parser);

	return true;
}

static bool gz_parser_reset(struct io_parser *parser)
{
	struct gz_parser_priv *priv = gz_parser_priv(parser);

	priv->state = CHECK_MAGIC;
	gz_close_zlib(parser);

	return io_parser_reset(priv->child);
}

static const struct io_parser_ops gz_parser_ops = {
	.parse = gz_parser_parse,
	.wait_queue = gz_parser_wait_queue,
	.destroy = gz_parser_destroy,
	.reset = gz_parser_reset,
};

/**
 * io_new_gz_parser - allocate and initialize new GZ parser
 */
struct io_parser *io_new_gz_parser(struct io_parser *child)
{
	struct gz_parser_priv *priv;

	if (!child)
		return NULL;

	priv = calloc(1, sizeof(*priv));
	if (!priv) {
		io_set_latest_error("%s():%d: calloc failed (errno: %d)",
				    __func__, __LINE__, errno);
		io_parser_destroy(child);
		return NULL;
	}

	priv->child = child;
	priv->state = CHECK_MAGIC;
	io_parser_init(&priv->parser, &gz_parser_ops);
	io_parser_reset(&priv->parser);

	return &priv->parser;
}
