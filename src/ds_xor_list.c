/*
 * XOR linked list
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
#include <assert.h>

#include "ds.h"

#ifndef DEBUG
	#define DISABLE_RUNTIME_TESTS
	#define ds_assert(x)
#else
	#define ds_assert(x) assert(x)
#endif

/**
 * ds_xorlist_entry_alloc - allocate memory area large enough for xorlist_entry
 *			    and data
 * @datasize: size of xorlist entry buffer
 */
struct ds_xorlist_entry *ds_xorlist_entry_alloc(size_t datasize)
{
	return calloc(1, sizeof(struct ds_xorlist_entry) + datasize);
}

/**
 * ds_xorlist_append_entry - add new xorlist entry element at end of xor list
 * @list: xor linked list
 * @entry: new xorlist entry
 */
void ds_xorlist_append_entry(struct ds_xor_list *list,
			     struct ds_xorlist_entry *entry)
{
	if (list->count++ > 0) {
		entry->prevnext = (uintptr_t)list->tail;

		list->tail->prevnext ^= (uintptr_t)entry;
		list->tail = entry;
	} else {
		entry->prevnext = (uintptr_t)entry ^ (uintptr_t)entry;

		list->head = entry;
		list->tail = entry;
	}
}

/**
 * ds_xorlist_prepend_entry - add new xorlist entry element at start of xor list
 * @list: xor linked list
 * @entry: new xorlist entry
 */
void ds_xorlist_prepend_entry(struct ds_xor_list *list,
			      struct ds_xorlist_entry *entry)
{
	if (list->count++ > 0) {
		entry->prevnext = (uintptr_t)list->head;

		list->head->prevnext ^= (uintptr_t)entry;
		list->head = entry;
	} else {
		entry->prevnext = (uintptr_t)entry ^ (uintptr_t)entry;

		list->head = entry;
		list->tail = entry;
	}
}

static struct ds_xorlist_entry *alloc_entry(void *data)
{
	struct ds_xorlist_entry *entry;

	ds_new_xorlist_entry(entry, void *, data);

	return entry;
}

/**
 * ds_xorlist_append - add new data element at end of xor list
 * @list: xor linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
bool ds_xorlist_append(struct ds_xor_list *list, void *data)
{
	struct ds_xorlist_entry *entry = alloc_entry(data);

	if (!entry)
		return false;

	ds_xorlist_append_entry(list, entry);
	return true;
}

/**
 * ds_xorlist_prepend - add new data element at start of xor list
 * @list: xor linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
bool ds_xorlist_prepend(struct ds_xor_list *list, void *data)
{
	struct ds_xorlist_entry *entry = alloc_entry(data);

	if (!entry)
		return false;

	ds_xorlist_prepend_entry(list, entry);
	return true;
}

/**
 * ds_xorlist_remove_entry - remove entry from xor list
 * @list: xor linked list
 * @entry: entry of xor list to be removed
 * @prev: entry next to entry item to be removed
 *
 * Entry element is removed from list but not freed.
 */
void ds_xorlist_remove_entry(struct ds_xor_list *list,
			     struct ds_xorlist_entry *entry,
			     struct ds_xorlist_entry *prev)
{
	struct ds_xorlist_entry *next;

	next = ds_xorlist_next(prev, entry);

	/*
	 * Input:
	 *	prev => B
	 *	prev->prevnext => A ^ C
	 *	entry => C
	 *	entry->prevnext => B ^ D
	 *	next => D
	 *	next->prevnext => C ^ E
	 * Output:
	 *	prev => B
	 *	prev->prevnext => A ^ D
	 *	next => D
	 *	next->prevnext => B ^ E
	 *
	 * Calculation:
	 *	prev->prevnext ^= entry ^ next
	 *	next->prevnext ^= entry ^ prev
	 */

	ds_assert(list->count > 0);

	if (entry == list->head && entry == list->tail) {
		/* only entry of list */
		list->head = NULL;
		list->tail = NULL;
	} else if (entry == list->head) {
		/* first of list */
		list->head = ds_xorlist_next(NULL, entry);
		next->prevnext ^= (uintptr_t)entry;
	} else if (entry == list->tail) {
		/* last of list */
		list->tail = ds_xorlist_next(NULL, entry);
		next->prevnext ^= (uintptr_t)entry;
	} else {
		/* middle */
		prev->prevnext ^= (uintptr_t)entry ^ (uintptr_t)next;
		next->prevnext ^= (uintptr_t)entry ^ (uintptr_t)prev;
	}

	list->count--;

#ifndef DISABLE_RUNTIME_TESTS
	/* Debugging and testing */
	ds_assert(list->count >= 0);
	if (list->head == NULL)
		ds_assert(list->tail == NULL);
	if (list->tail == NULL)
		ds_assert(list->head == NULL);
	if (list->count == 0)
		ds_assert(list->head == NULL);
	if (list->count == 1) {
		ds_assert(list->head == list->tail);
	}
	if (list->count > 1) {
		ds_assert(list->head != list->tail);
		ds_assert(list->head->prevnext != 0);
		ds_assert(list->tail->prevnext != 0);
	}
#endif
}

/**
 * ds_xorlist_delete_entry - delete entry from xor list
 * @list: xor linked list
 * @entry: entry of xor list to be deleted
 *
 * Entry element is also freed.
 */
void ds_xorlist_delete_entry(struct ds_xor_list *list,
			     struct ds_xorlist_entry *entry,
			     struct ds_xorlist_entry *prev)
{
	ds_xorlist_remove_entry(list, entry, prev);

#ifndef DISABLE_RUNTIME_TESTS
	/* Debugging and testing */
	memset(entry, 0xCC, sizeof(*entry));
#endif

	free(entry);
}

/**
 * ds_xorlist_find - find entry for data-pointer from xor list
 * @list: xor linked list
 * @data: data pointer to search list for
 */
struct ds_xorlist_entry *ds_xorlist_find(struct ds_xor_list *list,
					 const void *data)
{
	struct ds_xorlist_entry *pos, *prev;

	ds_xorlist_for_each(prev, pos, list)
		if (ds_xorlist_entry_data(void *, pos) == data)
			return pos;

	return NULL;
}

/**
 * ds_xorlist_purge - free xor list and all data elements
 * @list: xor linked list to be freed
 * @data_free: function callback used to free data elements. If NULL, data
 *             elements will not be freed.
 */
void ds_xorlist_purge(struct ds_xor_list *list, void(*data_free)(void *data))
{
	struct ds_xorlist_entry *pos, *next, *prev;

	if (ds_xorlist_empty(list))
		return;

	pos = list->head;
	next = ds_xorlist_next(NULL, pos);

	while (pos != NULL) {
		if (data_free)
			(*data_free)(ds_xorlist_entry_data(void *, pos));

		free(pos);

		prev = pos;
		pos = next;
		next = next ? ds_xorlist_next(prev, next) : NULL;
	}

	ds_xorlist_init(list);
}
