/*
 * Appendable buffer, scatter/gather memory buffer with ability to append new
 * data.
 *
 * Copyright © 2012 Jussi Kivilinna <jussi.kivilinna@mbnet.fi>
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
#include <memory.h>

#include "ds.h"

#define DS_APPEND_BUFFER_PIECE_LEN 256

#if DS_APPEND_BUFFER_PIECE_LEN <= 256
	typedef unsigned char piece_datalen_t;
#elif DS_APPEND_BUFFER_PIECE_LEN <= 65536
	typedef unsigned short piece_datalen_t;
#else
	typedef unsigned int piece_datalen_t;
#endif

#define DS_APPEND_BUFFER_PIECE_DATA_LEN (DS_APPEND_BUFFER_PIECE_LEN - \
						sizeof(struct ds_list_entry) - \
						sizeof(unsigned int))

struct ds_append_buffer_piece {
	struct ds_list_entry entry;
	piece_datalen_t datalen;
	unsigned char data[DS_APPEND_BUFFER_PIECE_DATA_LEN];
};

static inline struct ds_append_buffer_piece *
iterator_to_piece(struct ds_append_buffer_iterator *iter)
{
	/*
	 * iter->pchar and iter->ppos have enough offset information to
         * reconstruct piece pointer.
	 */
	return ds_container_of(iter->pchar - iter->ppos,
			       struct ds_append_buffer_piece, data);
}

static inline struct ds_append_buffer_piece *
entry_to_piece(struct ds_list_entry *entry)
{
	return ds_container_of(entry, struct ds_append_buffer_piece, entry);
}

/**
 * ds_append_buffer_free - free internal data structures and data of buffer
 * @buf: buffer to free
 */
void ds_append_buffer_free(struct ds_append_buffer *buf)
{
	struct ds_append_buffer_piece *piece;

	while (!ds_list_empty(&buf->list)) {
		/* get buffer piece through list entry */
		piece = entry_to_piece(ds_list_first(&buf->list));

		/* remove list entry */
		ds_list_remove_entry(&buf->list, &piece->entry);

		/* adjust total length */
		buf->length -= piece->datalen - buf->first_offset;
		buf->first_offset = 0;

		/* free buffer piece */
		free(piece);
	}
}

/**
 * ds_append_buffer_move - move internal data structures from one buffer to
 *			   another, also clears the old buffer.
 * @new_buf: buffer which data is moved to
 * @old_buf: buffer where data is copied from, and buffer which is
 *           cleared/reinitialized
 */
void ds_append_buffer_move(struct ds_append_buffer *new,
			   struct ds_append_buffer *old)
{
	/* Copy header (pointers etc) */
	*new = *old;

	/* Clear/reinitialize old header */
	ds_append_buffer_init(old);
}

/**
 * ds_append_buffer_clone - makes copy of @old_buf to @new_buf.
 * @new_buf: empty buffer
 * @old_buf: buffer to be cloned
 *
 * Return false in case of memory allocation failure. Otherwise true.
 */
bool ds_append_buffer_clone(struct ds_append_buffer *new,
			       const struct ds_append_buffer *old)
{
	struct ds_list_entry *pos;
	struct ds_append_buffer_piece *old_piece;
	struct ds_append_buffer_piece *new_piece;

	/* Copy header and reinitialize the list structure */
	*new = *old;
	ds_list_init(&new->list);

	ds_list_for_each(pos, &old->list) {
		old_piece = entry_to_piece(pos);

		/* Allocate new clone piece */
		new_piece = malloc(sizeof(*new_piece));
		if (!new_piece)
			return false;

		/* Copy data from old piece to new */
		memmove(new_piece->data, old_piece->data, old_piece->datalen);
		new_piece->datalen = old_piece->datalen;

		/* Add new piece at end of new piece list */
		ds_list_append_entry(&new->list, &new_piece->entry);
	}

	return true;
}

/**
 * copy_to_append_buffer_piece - copies data to one appendable buffer piece
 * @piece: appendable buffer piece
 * @buf: input buffer
 * @len: length of @buf
 *
 * Returns number of bytes copied
 */
static unsigned int
copy_to_append_buffer_piece(struct ds_append_buffer_piece *piece,
			    const unsigned char *buf, unsigned int len)
{
	unsigned int space_left;

	/* Check if input buffer can fully fit this piece */
	space_left = sizeof(piece->data) - piece->datalen;
	if (len <= space_left) {
		memmove(piece->data + piece->datalen, buf, len);
		piece->datalen += len;

		return len;
	}

	/* Input buffer cannot fully fit this buffer piece */
	memmove(piece->data + piece->datalen, buf, space_left);
	piece->datalen += space_left;

	return space_left;
}

/**
 * ds_append_buffer_append - appends data from @inbuf at end of data in @abuf
 * @abuf: appendable buffer
 * @inbuf: input buffer with new data
 * @len: length of @inbuf
 *
 * Returns number of bytes copied, should always be @len except in case of
 * memory allocation failure.
 */
unsigned int ds_append_buffer_append(struct ds_append_buffer *abuf,
				     const void *__inbuf, unsigned int len)
{
	struct ds_list_entry *list_last;
	struct ds_append_buffer_piece *last;
	const unsigned char *inbuf = __inbuf;
	unsigned int bytes_copied, total_copied;

	total_copied = 0;

	/* Nothing to do? */
	if (len == 0)
		return total_copied;

	/* First try to append at end of last entry */
	list_last = ds_list_last(&abuf->list);
	if (list_last) {
		last = entry_to_piece(list_last);

		/* Copy as many bytes to this buffer piece we can */
		bytes_copied = copy_to_append_buffer_piece(last, inbuf, len);
		abuf->length += bytes_copied;
		len -= bytes_copied;
		inbuf += bytes_copied;
		total_copied += bytes_copied;

		/* Everything fitted in, done */
		if (len == 0)
			return total_copied;
	}

	/* Buffer did not fit in to last buffer piece, create new pieces */
	do {
		last = malloc(sizeof(*last));
		if (!last) {
			/* out of memory! */
			return total_copied;
		}

		/* Initialize piece and add to piece list */
		last->datalen = 0;
		ds_list_append_entry(&abuf->list, &last->entry);

		/* Copy new buffer data to new piece */
		bytes_copied = copy_to_append_buffer_piece(last, inbuf, len);

		abuf->length += bytes_copied;
		len -= bytes_copied;
		inbuf += bytes_copied;
		total_copied += bytes_copied;
	} while (len > 0);

	return total_copied;
}

/**
 * ds_append_buffer_copy - copies data from appendable buffer
 * @abuf: appendable buffer
 * @offset: offset to @abuf where to start reading from
 * @buf: memory location where to write data
 * @len: size of read, length of @buf
 *
 * Returns number of bytes copied, will be less than @len if tried to read
 * pass the end of @abuf data.
 */
unsigned int ds_append_buffer_copy(struct ds_append_buffer *abuf,
				   unsigned int offset, void *buf,
				   unsigned int len)
{
	struct ds_append_buffer_piece *piece;
	struct ds_append_buffer_iterator iter;
	unsigned char *cbuf = buf;
	unsigned int piece_len, bytes_copied = 0;

	/* Initialize iterator for offset jump */
	ds_append_buffer_iterator_init(abuf, &iter);
	ds_append_buffer_iterator_forward(&iter, offset);

	/* offset is out of reach? */
	if (ds_append_buffer_iterator_has_reached_end(&iter))
		return bytes_copied;

	do {
		piece = iterator_to_piece(&iter);

		/* Does current piece contain all of needed bytes? */
		piece_len = piece->datalen - iter.ppos;
		if (piece_len >= len) {
			memmove(cbuf, piece->data + iter.ppos, len);

			bytes_copied += len;
			return bytes_copied;
		}

		/* Copy data from current piece */
		memmove(cbuf, piece->data + iter.ppos, piece_len);

		/* Proceed to next piece */
		ds_append_buffer_iterator_forward(&iter, piece_len);
		bytes_copied += piece_len;
		cbuf += piece_len;
		len -= piece_len;
	} while(!ds_append_buffer_iterator_has_reached_end(&iter));

	return bytes_copied;
}

/**
 * ds_append_buffer_move_head - moves head of buffer forward and frees memory
 *                              left unused
 * @abuf: appendable buffer
 * @add: how many bytes to move head forward
 */
bool ds_append_buffer_move_head(struct ds_append_buffer *abuf, unsigned int add)
{
	struct ds_list_entry *prev;
	struct ds_append_buffer_piece *piece, *prev_piece;
	struct ds_append_buffer_iterator iter;

	/* Allow moving head at end of buffer */
	if (add == abuf->length) {
		ds_append_buffer_free(abuf);
		return true;
	} else if (add > abuf->length) {
		/* In error case, clear buffer anyway but return false */
		ds_append_buffer_free(abuf);
		return false;
	}

	/* Initialize iterator and get new begining */
	ds_append_buffer_iterator_init(abuf, &iter);
	ds_append_buffer_iterator_forward(&iter, add);

	/*
	 * Set this piece as first piece of appendable buffer by removing
         * previous pieces from linked list and freeing them.
	 */
	piece = iterator_to_piece(&iter);
	prev = piece->entry.prev;
	while (prev) {
		prev_piece = entry_to_piece(prev);

		/* remove list entry */
		ds_list_remove_entry(&abuf->list, prev);

		/* get next prev */
		prev = prev_piece->entry.prev;

		/* free buffer piece */
		free(prev_piece);
	}

	abuf->first_offset = iter.ppos;
	abuf->length -= add;

	/* Free buffer when reached end */
	if (abuf->length == 0 && !ds_list_empty(&abuf->list))
		ds_append_buffer_free(abuf);

	return true;
}


/**
 * ds_append_buffer_append_piece - append piece_buf at end of appendable buffer.
 * 				   NOTE: appendable buffer must not have
 *				   trailing unused bytes when calling this
 *				   function.
 * 				   Use ds_append_buffer_get_end_free() to check
 * 				   that number of unused bytes is zero.
 * @abuf: appendable buffer
 * @piece_buf: buffer to add, must be allocated by using
 *             ds_append_buffer_new_piece()
 * @buflen: bytes in use in piece_buf
 */
bool ds_append_buffer_append_piece(struct ds_append_buffer *abuf,
				   void *piece_buf, unsigned int buflen)
{
	struct ds_append_buffer_piece *piece =
		ds_container_of(piece_buf, struct ds_append_buffer_piece, data);
	struct ds_list_entry *lentry = ds_list_last(&abuf->list);

	if (lentry) {
		struct ds_append_buffer_piece *last = entry_to_piece(lentry);
		/* check that last entry does not have used bytes */
		if (last->datalen < sizeof(last->data))
			return false;
	}

	piece->datalen = buflen;

	/* add new piece and adjust buffer length */
	ds_list_append_entry(&abuf->list, &piece->entry);
	abuf->length += buflen;

	return true;
}

/**
 * ds_append_buffer_new_piece - allocate new internal data buffer and return
 *                              pointer to it's data array.
 * @buflen: number of bytes in data array is stored here
 */
void *ds_append_buffer_new_piece(unsigned int *buflen)
{
	struct ds_append_buffer_piece *piece;

	piece = malloc(sizeof(*piece));
	if (!piece)
		return NULL;

	piece->datalen = 0;
	*buflen = sizeof(piece->data);

	return piece->data;
}

/**
 * ds_append_buffer_free_piece - free unused piece that was allocated
 * @piece_buf: pointer to data array of piece
 */
void ds_append_buffer_free_piece(void *piece_buf)
{
	struct ds_append_buffer_piece *piece =
		ds_container_of(piece_buf, struct ds_append_buffer_piece, data);

	free(piece);
}

/**
 * ds_append_buffer_get_end_free - Get unused memory at end of append_buffer
 * @abuf: appendable buffer
 * @freelen: length of returned buffer
 */
void *ds_append_buffer_get_end_free(struct ds_append_buffer *abuf,
				    unsigned int *freelen)
{
	struct ds_list_entry *lentry = ds_list_last(&abuf->list);
	struct ds_append_buffer_piece *piece;

	/* empty buffer? */
	if (!lentry) {
		*freelen = 0;
		return NULL;
	}

	piece = entry_to_piece(lentry);

	/*
	 * return free/unused part of last piece (or null if no free space
	 * left)
	 */
	*freelen = sizeof(piece->data) - piece->datalen;
	if (*freelen == 0)
		return NULL;
	return &piece->data[piece->datalen];
}

/**
 * ds_append_buffer_move_end - moves end of buffer forward (marks unused
 *			       trailing buffer as in use)
 * @abuf: appendable buffer
 * @add: how many bytes to move end forward, maximum move is @freelen returned
 *	 by ds_append_buffer_get_end_free()
 */
bool ds_append_buffer_move_end(struct ds_append_buffer *abuf, unsigned int add)
{
	struct ds_list_entry *lentry = ds_list_last(&abuf->list);
	struct ds_append_buffer_piece *piece;

	/* empty buffer?! */
	if (!lentry)
		return false;

	piece = entry_to_piece(lentry);

	/* moving more than is possible?! */
	if (add > sizeof(piece->data) - piece->datalen)
		return false;

	piece->datalen += add;
	abuf->length += add;

	return true;
}

/**
 * ds_append_buffer_get_write_buffer - get direct-write-buffer that will
 *				       result writing directly to appendable
 *				       buffer
 * @abuf: appendable buffer
 * @buflen: length of returned write buffer
 */
void *ds_append_buffer_get_write_buffer(struct ds_append_buffer *abuf,
					unsigned int *buflen)
{
	void *piece_buf;

	piece_buf = ds_append_buffer_get_end_free(abuf, buflen);
	if (!piece_buf) {
		/*
		 * All memory used by append_buffer is fully utilized, allocate
		 * new internal buffer structure.
		 */
		piece_buf = ds_append_buffer_new_piece(buflen);
		if (!piece_buf) {
			/* out of mem */
			return NULL;
		}
	}

	return piece_buf;
}

/**
 * ds_append_buffer_finish_write_buffer - finish direct-write-buffer (either
					  append or free)
 * @abuf: appendable buffer
 * @write_buf: write buffer where caller has written new appended bytes
 * @buflen: number of used bytes in write buffer
 */
bool ds_append_buffer_finish_write_buffer(struct ds_append_buffer *abuf,
					  void *write_buf, unsigned int buflen)
{
	struct ds_list_entry *lentry = ds_list_last(&abuf->list);
	struct ds_append_buffer_piece *piece;

	/* Check if write_buf is already part of @abuf */
	if (lentry) {
		piece = entry_to_piece(lentry);
		if ((unsigned long)write_buf >= (unsigned long)piece->data &&
		    (unsigned long)write_buf <
			(unsigned long)&piece->data[sizeof(piece->data)]) {
			/* finish by moving end pointer */
			return ds_append_buffer_move_end(abuf, buflen);
		}
	}

	/* append new piece to @abuf */
	return ds_append_buffer_append_piece(abuf, write_buf, buflen);
}

/**
 * ds_append_buffer_iterator_init - initialize appendable buffer iterator
 */
void ds_append_buffer_iterator_init(struct ds_append_buffer *abuf,
				    struct ds_append_buffer_iterator *iter)
{
	struct ds_list_entry *first;
	struct ds_append_buffer_piece *piece;

	iter->pchar = NULL;
	iter->ppos = abuf->first_offset;
	iter->pos = 0;
	iter->pmax = 0;

	if (ds_append_buffer_length(abuf) == 0)
		return;

	first = ds_list_first(&abuf->list);
	if (!first)
		return;

	piece = entry_to_piece(first);
	iter->pchar = piece->data + iter->ppos;
	iter->pmax = piece->datalen;
}

/**
 * __ds_append_buffer_iterator_forward - slow path for moving iterator forward
 *					 by @add bytes
 * @iter: iterator structure
 * @add: number of bytes to move iterator forward
 */
void __ds_append_buffer_iterator_forward(
		struct ds_append_buffer_iterator *iter, unsigned int add)
{
	struct ds_append_buffer_piece *piece;
	struct ds_list_entry *next;
	unsigned int steps_left;

	piece = iterator_to_piece(iter);

	do {
		/* Check if we can stop iterating piece list now */
		steps_left = piece->datalen - iter->ppos;
		if (add < steps_left) {
			/* Update iterator structure with new position */
			iter->ppos += add;
			iter->pos += add;
			iter->pchar += add;

			return;
		}

		/* Proceed to next piece */
		next = piece->entry.next;

		/* Update interator structure with new position*/
		iter->pos += steps_left;
		iter->pchar = NULL;
		iter->ppos = 0;

		if (!next)
			return;

		piece = entry_to_piece(next);
		iter->pchar = piece->data;
		iter->pmax = piece->datalen;
		add -= steps_left;
	} while (add > 0);
}
