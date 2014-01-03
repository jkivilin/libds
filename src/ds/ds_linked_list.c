/*
 * Doubly linked list
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
 * ds_list_entry_alloc - allocate memory area large enough for list_entry and
 *			 data
 * @datasize: size of list entry buffer
 */
struct ds_list_entry *ds_list_entry_alloc(size_t datasize)
{
	return calloc(1, sizeof(struct ds_list_entry) + datasize);
}

/**
 * ds_list_append_entry - add new list entry element at end of list
 * @list: linked list
 * @entry: new list entry
 */
void ds_list_append_entry(struct ds_linked_list *list,
			  struct ds_list_entry *entry)
{
	if (list->count++ > 0) {
		entry->next = NULL;
		entry->prev = list->tail;

		list->tail->next = entry;
		list->tail = entry;
	} else {
		entry->next = NULL;
		entry->prev = NULL;

		list->head = entry;
		list->tail = entry;
	}
}

/**
 * ds_list_prepend_entry - add new list entry element at start of list
 * @list: linked list
 * @entry: new list entry
 */
void ds_list_prepend_entry(struct ds_linked_list *list,
			   struct ds_list_entry *entry)
{
	if (list->count++ > 0) {
		entry->next = list->head;
		entry->prev = NULL;

		list->head->prev = entry;
		list->head = entry;
	} else {
		entry->next = NULL;
		entry->prev = NULL;

		list->head = entry;
		list->tail = entry;
	}
}

static struct ds_list_entry *alloc_entry(void *data)
{
	struct ds_list_entry *entry;

	ds_new_list_entry(entry, void *, data);

	return entry;
}

/**
 * ds_list_append - add new data element at end of list
 * @list: linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
bool ds_list_append(struct ds_linked_list *list, void *data)
{
	struct ds_list_entry *entry = alloc_entry(data);

	if (!entry)
		return false;

	ds_list_append_entry(list, entry);
	return true;
}

/**
 * ds_list_prepend - add new data element at start of list
 * @list: linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
bool ds_list_prepend(struct ds_linked_list *list, void *data)
{
	struct ds_list_entry *entry = alloc_entry(data);

	if (!entry)
		return false;

	ds_list_prepend_entry(list, entry);
	return true;
}

/**
 * ds_list_remove_entry - remove entry from list
 * @list: linked list
 * @entry: entry of list to be removed
 *
 * Entry element is removed from list but not freed.
 */
void ds_list_remove_entry(struct ds_linked_list *list,
			  struct ds_list_entry *entry)
{
	ds_assert(list->count > 0);

	if (entry == list->head && entry == list->tail) {
		/* only entry of list */
		list->head = NULL;
		list->tail = NULL;
	} else if (entry == list->head) {
		/* first of list */
		list->head = entry->next;
		entry->next->prev = NULL;
	} else if (entry == list->tail) {
		/* last of list */
		list->tail = entry->prev;
		entry->prev->next = NULL;
	} else {
		/* middle */
		entry->next->prev = entry->prev;
		entry->prev->next = entry->next;
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
	if (list->count > 0) {
		ds_assert(list->head->prev == NULL);
		ds_assert(list->tail->next == NULL);
	}
	if (list->count == 1) {
		ds_assert(list->head == list->tail);
	}
	if (list->count > 1) {
		ds_assert(list->head != list->tail);
		ds_assert(list->head->next != NULL);
		ds_assert(list->tail->prev != NULL);
	}
#endif
}

/**
 * ds_list_delete_entry - delete entry from list
 * @list: linked list
 * @entry: entry of list to be deleted
 *
 * Entry element is also freed.
 */
void ds_list_delete_entry(struct ds_linked_list *list,
			  struct ds_list_entry *entry)
{
	ds_list_remove_entry(list, entry);

#ifndef DISABLE_RUNTIME_TESTS
	/* Debugging and testing */
	memset(entry, 0xCC, sizeof(*entry));
#endif

	free(entry);
}

/**
 * ds_list_find - find entry for data-pointer from list
 * @list: linked list
 * @data: data pointer to search list for
 */
struct ds_list_entry *ds_list_find(struct ds_linked_list *list,
				   const void *data)
{
	struct ds_list_entry *pos;

	ds_list_for_each(pos, list)
		if (ds_list_entry_data(void *, pos) == data)
			return pos;

	return NULL;
}

/**
 * ds_list_purge - free list and all data elements
 * @list: linked list to be freed
 * @data_free: function callback used to free data elements. If NULL, data
 *             elements will not be freed.
 */
void ds_list_purge(struct ds_linked_list *list,
		   void(*data_free)(void *data))
{
	struct ds_list_entry *pos, *next;

	if (ds_list_empty(list))
		return;

	for (pos = list->head, next = pos->next; pos != NULL;
				pos = next, next = next ? next->next : NULL) {
		if (data_free)
			(*data_free)(ds_list_entry_data(void *, pos));

		free(pos);
	}

	ds_list_init(list);
}
