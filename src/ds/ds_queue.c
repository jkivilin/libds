/*
 * Queue, simple implementation by reusing linked_list
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
#include <memory.h>

#include "ds.h"

/**
 * __ds_queue_peek_data - peek last data buffer of queue but do not pop
 * @q: queue
 * @buf: data buffer
 * @buflen: length of buffer
 *
 * Return false if queue is empty, true if data was copied over to buffer
 */
bool __ds_queue_peek_data(struct ds_queue *q, void *buf, size_t buflen)
{
	struct ds_list_entry *last = ds_queue_peek_entry(q);

	if (!last)
		return false;

	memmove(buf, ds_list_entry_data_ptr(void, last), buflen);
	return true;
}

/**
 * ds_queue_pop_entry - pop entry from end of queue
 * @q: queue
 *
 * Return NULL if queue is empty.
 */
struct ds_list_entry *ds_queue_pop_entry(struct ds_queue *q)
{
	struct ds_list_entry *last = ds_list_last(&q->q);

	if (!last)
		return NULL;

	ds_list_remove_entry(&q->q, last);
	return last;
}

/**
 * ds_queue_pop_ptr - pop data element from end of queue
 * @q: queue
 *
 * Return NULL if queue is empty (or if data element was NULL).
 */
void *ds_queue_pop_ptr(struct ds_queue *q)
{
	struct ds_list_entry *last = ds_queue_pop_entry(q);
	void *data;

	if (!last)
		return NULL;

	data = ds_list_entry_data(void *, last);
	free(last);

	return data;
}

/**
 * __ds_queue_pop_data - pop data buffer element from end of queue
 * @q: queue
 * @buf: data buffer
 * @buflen: length of buffer
 *
 * Return false if queue is empty, true if data was copied over to buffer
 */
bool __ds_queue_pop_data(struct ds_queue *q, void *buf, size_t buflen)
{
	struct ds_list_entry *last = ds_queue_pop_entry(q);

	if (!last)
		return false;

	memmove(buf, ds_list_entry_data_ptr(void, last), buflen);
	free(last);

	return true;
}

/**
 * ds_queue_free - frees queue entries
 * @q: queue
 */
void ds_queue_free(struct ds_queue *q)
{
	struct ds_list_entry *entry;

	while (ds_queue_size(q) > 0) {
		entry = ds_queue_pop_entry(q);
		if (entry)
			free(entry);
	}
}

