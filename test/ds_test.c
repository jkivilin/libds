/*
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
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "ds.h"

static int __ds_test_assert(const bool check, const int line, const char *check_str)
{
	if (__builtin_expect(!!check, 1))
		return 0;

	printf("[ASSERT FAILED!]\n"
		"\tline %d: Assertion `%s' failed.\n", line, check_str);
	fflush(stdout);
	fflush(stderr);

	return -1;
}

#define ds_test_assert(should_be_true) do { \
		int __ret = __ds_test_assert(should_be_true, __LINE__, #should_be_true); \
		if (__builtin_expect(__ret < 0, 0)) \
			return __ret; \
	} while(false)

static int ds_basic_test(void)
{
	struct ds_basic_test {
		unsigned char buf0[4];
		unsigned char buf4[4];
		unsigned char buf8[1];
		unsigned char buf9[1];
	} s;
	void *ptr;

	/* test ds_test_assert, true and false */
	ds_test_assert(true);
	ds_test_assert(true != 0);
	ds_test_assert(!true == 0);
	ds_test_assert(!(false));
	ds_test_assert(false == 0);
	ds_test_assert(!false == !0);
	ds_test_assert(!!false == !!0);
	ds_test_assert(false == !true);
	ds_test_assert(!false == true);

	/* test ds_offset_of */
	ds_test_assert(ds_offset_of(struct ds_basic_test, buf0) == 0);
	ds_test_assert(ds_offset_of(struct ds_basic_test, buf4) == 4);
	ds_test_assert(ds_offset_of(struct ds_basic_test, buf8) == 8);
	ds_test_assert(ds_offset_of(struct ds_basic_test, buf9) == 9);

	/* test ds_container_of */
	ptr = s.buf0;
	ds_test_assert(ds_container_of(ptr, struct ds_basic_test, buf0) == &s);
	ptr = s.buf4;
	ds_test_assert(ds_container_of(ptr, struct ds_basic_test, buf4) == &s);
	ptr = s.buf8;
	ds_test_assert(ds_container_of(ptr, struct ds_basic_test, buf8) == &s);
	ptr = s.buf9;
	ds_test_assert(ds_container_of(ptr, struct ds_basic_test, buf9) == &s);

	return 0;
}

static int ds_linked_list_test_purge_callback_counter;
static int ds_linked_list_test_purge_callback_errors;

static void ds_linked_list_test_purge_callback(void *data)
{
	long count = (long)data;

	if (!(count == ds_linked_list_test_purge_callback_counter++))
		ds_linked_list_test_purge_callback_errors++;
}

static int ds_linked_list_test(void)
{
	struct ds_linked_list list;
	struct ds_list_entry *pos;
	long i;

	/* test list initialization */
	ds_list_init(&list);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* test freeing list entries of empty list */
	ds_list_free(&list);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* test adding entry at tail of empty list */
	ds_list_append(&list, (void *)1);
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_first(&list) == ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)1);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 1);

	/* test removing the only entry of list */
	ds_list_delete_entry(&list, ds_list_first(&list));
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* test adding entry at head of empty list */
	ds_list_prepend(&list, (void *)2);
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_first(&list) == ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)2);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 1);
	ds_list_delete_entry(&list, ds_list_first(&list));

	/* test adding two entries at tail of list */
	ds_list_append(&list, (void *)1);
	ds_list_append(&list, (void *)2);
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_last(&list) != NULL);
	ds_test_assert(ds_list_first(&list) != ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)1);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void *)2);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 2);

	/* test removing the first entry of list */
	ds_list_delete_entry(&list, ds_list_first(&list));
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_first(&list) == ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)2);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 1);
	ds_list_delete_entry(&list, ds_list_first(&list));

	/* fill list */
	ds_list_prepend(&list, (void *)1);
	ds_list_append(&list, (void *)2);
	ds_list_prepend(&list, (void *)3);
	ds_list_append(&list, (void *)4);
	ds_list_prepend(&list, (void *)5);
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_last(&list) != NULL);
	ds_test_assert(ds_list_first(&list) != ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)5);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void *)4);
	ds_test_assert(ds_list_size(&list) == 5);

	/* verify forward iteration through list */
	i = 0;
	ds_list_for_each(pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)5);
			break;
		case 1:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)3);
			break;
		case 2:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)1);
			break;
		case 3:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)2);
			break;
		case 4:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)4);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* verify backward iteration through list */
	i = 0;
	ds_list_for_each_tail(pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)4);
			break;
		case 1:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)2);
			break;
		case 2:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)1);
			break;
		case 3:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)3);
			break;
		case 4:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)5);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* remove middle entry off list (note: don't use ds_list_find) */
	i = 0;
	ds_list_for_each(pos, &list)
		if (i++ == 2)
			break;

	ds_list_delete_entry(&list, pos);
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_last(&list) != NULL);
	ds_test_assert(ds_list_first(&list) != ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)5);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void *)4);
	ds_test_assert(ds_list_size(&list) == 4);

	/* verify structure of list */
	i = 0;
	ds_list_for_each(pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)5);
			break;
		case 1:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)3);
			break;
		case 2:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)2);
			break;
		case 3:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)4);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	i = 0;
	ds_list_for_each_tail(pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)4);
			break;
		case 1:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)2);
			break;
		case 2:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)3);
			break;
		case 3:
			ds_test_assert(ds_list_entry_data(void *, pos) == (void *)5);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* remove first and last of list */
	ds_list_delete_entry(&list, ds_list_first(&list));
	ds_list_delete_entry(&list, ds_list_last(&list));
	ds_test_assert(ds_list_first(&list) != NULL);
	ds_test_assert(ds_list_last(&list) != NULL);
	ds_test_assert(ds_list_first(&list) != ds_list_last(&list));
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void *)3);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void *)2);
	ds_test_assert(ds_list_size(&list) == 2);

	/* free list */
	ds_list_free(&list);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* test ds_list_purge callback */
	for (i = 1; i <= 1000; i++) {
		ds_list_append(&list, (void *)i);
		ds_test_assert(ds_list_size(&list) == i);
	}
	ds_linked_list_test_purge_callback_counter = 1;
	ds_linked_list_test_purge_callback_errors = 0;
	ds_list_purge(&list, ds_linked_list_test_purge_callback);
	ds_test_assert(ds_linked_list_test_purge_callback_counter == 1001);
	ds_test_assert(ds_linked_list_test_purge_callback_errors == 0);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* find entry for data pointer from list */
	for (i = 1; i <= 3; i++)
		ds_list_append(&list, (void *)i);
	pos = ds_list_find(&list, (void *)2);
	ds_test_assert(pos != NULL);
	ds_test_assert(ds_list_first(&list)->next == pos);
	ds_test_assert(ds_list_first(&list)->prev == NULL);
	ds_test_assert(ds_list_last(&list)->prev == pos);
	ds_test_assert(ds_list_last(&list)->next == NULL);
	ds_list_delete_entry(&list, pos);
	ds_test_assert(ds_list_first(&list)->next == ds_list_last(&list));
	ds_test_assert(ds_list_first(&list)->prev == NULL);
	ds_test_assert(ds_list_last(&list)->prev == ds_list_first(&list));
	ds_test_assert(ds_list_last(&list)->next == NULL);

	/* find returns null when not found */
	pos = ds_list_find(&list, (void *)2);
	ds_test_assert(pos == NULL);

	/* free list */
	ds_list_free(&list);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	/* find returns null when not found */
	pos = ds_list_find(&list, (void *)1);
	ds_test_assert(pos == NULL);

	/* test link_remove_entry & link_append_entry & link_prepend_entry */
	ds_list_prepend(&list, (void*)2);
	ds_list_prepend(&list, (void*)1);
	pos = ds_list_last(&list);

	ds_list_remove_entry(&list, pos);
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void*)1);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void*)1);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 1);
	ds_test_assert(ds_list_entry_data(void *, pos) == (void*)2);

	ds_list_append_entry(&list, pos);
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void*)1);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void*)2);
	ds_test_assert(ds_list_last(&list) == pos);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 2);

	pos = ds_list_first(&list);
	ds_list_remove_entry(&list, pos);
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void*)2);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void*)2);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 1);
	ds_test_assert(ds_list_entry_data(void *, pos) == (void*)1);

	ds_list_prepend_entry(&list, pos);
	ds_test_assert(ds_list_entry_data(void *, ds_list_first(&list)) == (void*)1);
	ds_test_assert(ds_list_entry_data(void *, ds_list_last(&list)) == (void*)2);
	ds_test_assert(ds_list_first(&list) == pos);
	ds_test_assert(!ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 2);

	/* free list */
	ds_list_free(&list);
	ds_test_assert(ds_list_first(&list) == NULL);
	ds_test_assert(ds_list_last(&list) == NULL);
	ds_test_assert(ds_list_empty(&list));
	ds_test_assert(ds_list_size(&list) == 0);

	return 0;
}

static int ds_xor_list_test(void)
{
	struct ds_xor_list list;
	struct ds_xorlist_entry *pos, *prev;
	long i;

	/* test list initialization */
	ds_xorlist_init(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test freeing list entries of empty list */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test adding entry at tail of empty list */
	ds_xorlist_append(&list, (void *)1);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) == ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)1);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);

	/* test freeing list entry */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test adding two entries at tail of empty list */
	ds_xorlist_append(&list, (void *)1);
	ds_xorlist_append(&list, (void *)2);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) != ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)1);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void *)2);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 2);
	ds_test_assert(ds_xorlist_next(NULL, ds_xorlist_first(&list)) == ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_next(NULL, ds_xorlist_last(&list)) == ds_xorlist_first(&list));

	/* test freeing list entries */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test adding two entries at tail of empty list */
	ds_xorlist_append(&list, (void *)1);
	ds_xorlist_append(&list, (void *)2);
	ds_xorlist_append(&list, (void *)3);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) != ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)1);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void *)3);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 3);

	/* Test manual iteration through list of three elements */
	pos = ds_xorlist_next(NULL, ds_xorlist_first(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
	pos = ds_xorlist_next(ds_xorlist_first(&list), pos);
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)3);
	pos = ds_xorlist_next(NULL, ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
	pos = ds_xorlist_next(ds_xorlist_last(&list), pos);
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)1);

	/* test freeing list entries */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test adding entry at tail of empty list */
	ds_xorlist_append(&list, (void *)1);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) == ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)1);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);

	/* test removing the only entry of list */
	ds_xorlist_delete_entry(&list, ds_xorlist_first(&list), NULL);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test adding entry at head of empty list */
	ds_xorlist_prepend(&list, (void *)2);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) == ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)2);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);
	ds_xorlist_delete_entry(&list, ds_xorlist_first(&list), NULL);

	/* test removing the first entry of list */
	ds_xorlist_append(&list, (void *)1);
	ds_xorlist_append(&list, (void *)2);
	ds_xorlist_delete_entry(&list, ds_xorlist_first(&list), NULL);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) == ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)2);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);
	ds_xorlist_delete_entry(&list, ds_xorlist_first(&list), NULL);


	/* fill list */
	ds_xorlist_prepend(&list, (void *)1);
	ds_xorlist_append(&list, (void *)2);
	ds_xorlist_prepend(&list, (void *)3);
	ds_xorlist_append(&list, (void *)4);
	ds_xorlist_prepend(&list, (void *)5);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_last(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) != ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)5);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void *)4);
	ds_test_assert(ds_xorlist_size(&list) == 5);

	/* verify forward iteration through list */
	i = 0;
	ds_xorlist_for_each(prev, pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)5);
			break;
		case 1:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)3);
			break;
		case 2:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)1);
			break;
		case 3:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
			break;
		case 4:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)4);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* verify backward iteration through list */
	i = 0;
	ds_xorlist_for_each_tail(prev, pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)4);
			break;
		case 1:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
			break;
		case 2:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)1);
			break;
		case 3:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)3);
			break;
		case 4:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)5);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* remove middle entry off list (note: don't use ds_xorlist_find) */
	i = 0;
	ds_xorlist_for_each(prev, pos, &list)
		if (i++ == 2)
			break;

	ds_xorlist_delete_entry(&list, pos, prev);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_last(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) != ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)5);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void *)4);
	ds_test_assert(ds_xorlist_size(&list) == 4);

	/* verify structure of list */
	i = 0;
	ds_xorlist_for_each(prev, pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)5);
			break;
		case 1:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)3);
			break;
		case 2:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
			break;
		case 3:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)4);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	i = 0;
	ds_xorlist_for_each_tail(prev, pos, &list) {
		switch(i++) {
		case 0:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)4);
			break;
		case 1:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)2);
			break;
		case 2:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)3);
			break;
		case 3:
			ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void *)5);
			break;
		default:
			ds_test_assert("too many list entries" == NULL);
			break;
		}
	}

	/* remove first and last of list */
	ds_xorlist_delete_entry(&list, ds_xorlist_first(&list), NULL);
	ds_xorlist_delete_entry(&list, ds_xorlist_last(&list), NULL);
	ds_test_assert(ds_xorlist_first(&list) != NULL);
	ds_test_assert(ds_xorlist_last(&list) != NULL);
	ds_test_assert(ds_xorlist_first(&list) != ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void *)3);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void *)2);
	ds_test_assert(ds_xorlist_size(&list) == 2);

	/* free list */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* test ds_xorlist_purge callback */
	for (i = 1; i <= 1000; i++) {
		ds_xorlist_append(&list, (void *)i);
		ds_test_assert(ds_xorlist_size(&list) == i);
	}
	ds_linked_list_test_purge_callback_counter = 1;
	ds_linked_list_test_purge_callback_errors = 0;
	ds_xorlist_purge(&list, ds_linked_list_test_purge_callback);
	ds_test_assert(ds_linked_list_test_purge_callback_counter == 1001);
	ds_test_assert(ds_linked_list_test_purge_callback_errors == 0);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* find entry for data pointer from list */
	for (i = 1; i <= 3; i++)
		ds_xorlist_append(&list, (void *)i);
	pos = ds_xorlist_find(&list, (void *)2);
	ds_test_assert(pos != NULL);
	ds_test_assert(ds_xorlist_first(&list)->prevnext == (uintptr_t)pos);
	ds_test_assert(ds_xorlist_last(&list)->prevnext == (uintptr_t)pos);
	ds_xorlist_delete_entry(&list, pos, ds_xorlist_first(&list));
	ds_test_assert(ds_xorlist_first(&list)->prevnext == (uintptr_t)ds_xorlist_last(&list));
	ds_test_assert(ds_xorlist_last(&list)->prevnext == (uintptr_t)ds_xorlist_first(&list));

	/* find returns null when not found */
	pos = ds_xorlist_find(&list, (void *)2);
	ds_test_assert(pos == NULL);

	/* free list */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	/* find returns null when not found */
	pos = ds_xorlist_find(&list, (void *)1);
	ds_test_assert(pos == NULL);

	/* test link_remove_entry & link_append_entry & link_prepend_entry */
	ds_xorlist_prepend(&list, (void*)2);
	ds_xorlist_prepend(&list, (void*)1);
	pos = ds_xorlist_last(&list);

	ds_xorlist_remove_entry(&list, pos, NULL);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void*)1);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void*)1);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void*)2);

	ds_xorlist_append_entry(&list, pos);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void*)1);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void*)2);
	ds_test_assert(ds_xorlist_last(&list) == pos);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 2);

	pos = ds_xorlist_first(&list);
	ds_xorlist_remove_entry(&list, pos, NULL);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void*)2);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void*)2);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 1);
	ds_test_assert(ds_xorlist_entry_data(void *, pos) == (void*)1);

	ds_xorlist_prepend_entry(&list, pos);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_first(&list)) == (void*)1);
	ds_test_assert(ds_xorlist_entry_data(void *, ds_xorlist_last(&list)) == (void*)2);
	ds_test_assert(ds_xorlist_first(&list) == pos);
	ds_test_assert(!ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 2);

	/* free list */
	ds_xorlist_free(&list);
	ds_test_assert(ds_xorlist_first(&list) == NULL);
	ds_test_assert(ds_xorlist_last(&list) == NULL);
	ds_test_assert(ds_xorlist_empty(&list));
	ds_test_assert(ds_xorlist_size(&list) == 0);

	return 0;
}

static int ds_queue_test(void)
{
	struct ds_list_entry *pos;
	struct ds_queue q;
	void *data;
	long i;
	char c = 0;

	/* test queue initialization */
	ds_queue_init(&q);
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* test freeing empty queue */
	ds_queue_free(&q);
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* test pushing data to queue */
	ds_queue_push_data(&q, char, 1);
	ds_test_assert(ds_queue_peek_data(&q, char, c) == 1);
	ds_test_assert(ds_queue_size(&q) == 1);

	/* test poping data from queue */
	ds_queue_pop_data(&q, char, c);
	ds_test_assert(c == 1);
	ds_test_assert(ds_queue_peek_data(&q, char, c) == 1);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* test pushing lots of entries */
	for (i = 0; i < 1000*10; i++) {
		ds_queue_push_ptr(&q, (void *)(i + 1));
		ds_test_assert(ds_queue_size(&q) == (i + 1));
		/* tail should always start same until pop */
		ds_test_assert(ds_queue_peek(&q) == (void *)1);
	}

	/* test poping all entries */
	for (i = 0; ds_queue_size(&q) > 0; i++) {
		data = ds_queue_pop_ptr(&q);

		ds_test_assert(data == (void *)(i + 1));
		ds_test_assert(ds_queue_size(&q) == (1000*10 - (i + 1)));
	}
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* test pushing lots of entries and free */
	for (i = 0; i < 1000*10; i++)
		ds_queue_push_ptr(&q, (void *)(i + 1));
	ds_queue_free(&q);
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* test pushing lots of entries and test ds_queue_for_each */
	for (i = 0; i < 1000*10; i++)
		ds_queue_push_ptr(&q, (void *)(i + 1));
	i = 1;
	ds_queue_for_each(pos, &q) {
		ds_test_assert(ds_list_entry_data(void *, pos) == (void *)i++);
	}
	ds_queue_free(&q);
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	/* tests for direct entry interface */
	for (i = 0; i < 1000*10; i++) {
		struct ds_list_entry *entry;
		ds_new_list_entry(entry, long double, i);

		ds_test_assert(entry != NULL);

		ds_queue_push_entry(&q, entry);
	}
	ds_test_assert(ds_queue_size(&q) == 1000*10);
	for (i = 0; i < 1000*10; i++) {
		struct ds_list_entry *entry1 = ds_queue_peek_entry(&q);
		struct ds_list_entry *entry2 = ds_queue_pop_entry(&q);

		ds_test_assert(entry1 == entry2);
		ds_test_assert(ds_list_entry_data(long double, entry1) == i);
		ds_test_assert(ds_queue_size(&q) == (1000*10 - (i + 1)));

		free(entry1);
	}
	ds_test_assert(ds_queue_peek(&q) == NULL);
	ds_test_assert(ds_queue_size(&q) == 0);

	return 0;
}

#define NUM_PUSHERS 10
#define NUM_PUSHS 1024
static void *ds_async_queue_queue_push_thread(void *__param)
{
	struct ds_async_queue *queue = __param;
	char buf[128];
	int i, slen;

	slen = snprintf(buf, sizeof(buf), "from thread id:%ld", (long)pthread_self());

	for (i = 0; i < NUM_PUSHS; i++)
		ds_async_queue_push(queue, buf, slen + 1);

	pthread_exit(NULL);
}

static void *ds_async_queue_queue_pop_thread(void *__param)
{
	struct ds_timespec abstime;
	struct ds_async_queue *queue = __param;
	void *__msg;
	char *msg;
	size_t msglen;
	int ret;
	long count = 0;

	while (count < NUM_PUSHS) {
		ret = ds_make_timeout_ms(&abstime, 50);
		assert(ret == 0); /* threaded.. need hard ds_test_assert */

		ret = ds_async_queue_pop_timed(queue, &__msg, &msglen, &abstime);
		msg = __msg;
		if (ret == -ETIMEDOUT)
			continue;

		free(msg);
		count++;
	}

	pthread_exit((void *)count);
}

int ds_async_queue_test(void)
{
	struct ds_async_queue *queue;
	pthread_t threads[NUM_PUSHERS * 2];
	char testbuf[32];
	struct ds_timespec abstime, now;
	void *__msg;
	char *msg;
	size_t msglen;
	int i, j, val, ret;
	void *status;
	bool ds_test_assertcheck;

	/* creation */
	queue = ds_async_queue_alloc();
	ds_test_assert(queue != NULL);
	ds_test_assert(ds_async_queue_empty(queue));

	/* pushing works? */
	/* queue has limited length will block when full, but should not block for first push. */
	ret = ds_async_queue_try_push(queue, "test", sizeof("test"));
	ds_test_assert(ret == 0);
	ds_test_assert(!ds_async_queue_empty(queue));

	/* poping works? */
	/* poping blocks on empty queue, but not now. */
	ret = ds_async_queue_try_pop(queue, &__msg, &msglen);
	msg = __msg;
	ds_test_assert(ret == 0);
	ds_test_assert(msglen == sizeof("test"));
	ds_test_assertcheck = strcmp(msg, "test") == 0;
	ds_test_assert(ds_test_assertcheck);
	free(msg);

	/* poping on empty queue blocks, test timeout 1 sec */
	ret = ds_get_curr_timespec(&now);
	ds_test_assert(ret == 0);

	abstime = now;
	abstime.tv_sec += 1;

	ret = ds_async_queue_pop_timed(queue, &__msg, &msglen, &abstime);
	msg = __msg;
	ds_test_assert(ret == -ETIMEDOUT);

	ret = ds_get_curr_timespec(&abstime);
	ds_test_assert(ret == 0);
	ds_test_assert(abstime.tv_sec >= now.tv_sec + 1);

	/* push too much to queue causes block */
	for (i = 0; i < 1000000; i++) {
		snprintf(testbuf, sizeof(testbuf), "%d", i);

		ret = ds_get_curr_timespec(&now);
		ds_test_assert(ret == 0);

		abstime = now;
		abstime.tv_sec += 1;

		ret = ds_async_queue_push_timed(queue, testbuf, strlen(testbuf) + 1, &abstime);
		if (ret == -ETIMEDOUT) {
			ret = ds_get_curr_timespec(&abstime);
			ds_test_assert(ret == 0);
			ds_test_assert(abstime.tv_sec >= now.tv_sec + 1);

			break;
		} else {
			ds_test_assert(ret == 0);
		}
	}
	ds_test_assert(i < 1000000); /* limit should have been hit much earlier */

	ds_test_assert(!ds_async_queue_empty(queue));

	/* pop until empty buffer */
	for (j = 0; j < i; j++) {
		ret = ds_async_queue_try_pop(queue, &__msg, &msglen);
		msg = __msg;
		if (ret == -ETIMEDOUT) {
			ds_test_assert(i == j);
			break;
		} else {
			ds_test_assert(ret == 0);
		}

		ret = sscanf(msg, "%d", &val);
		ds_test_assert(ret == 1);
		ds_test_assert(val == j);
		free(msg);
	}

	ds_test_assert(ds_async_queue_empty(queue));

	ds_async_queue_free(queue);

	/* Test multiple pushers */
	queue = ds_async_queue_alloc();

	for (i = 0; i < NUM_PUSHERS; i++) {
		ret = pthread_create(&threads[i], NULL, ds_async_queue_queue_push_thread, queue);
		ds_test_assert(ret == 0);
	}

	i = 0;
	do {
		ds_async_queue_pop(queue, &__msg, &msglen);
		msg = __msg;

		free(msg);

		i++;
	} while (i < NUM_PUSHS * NUM_PUSHERS);

	for (i = 0; i < NUM_PUSHERS; i++) {
		ret = pthread_join(threads[i], &status);
		ds_test_assert(ret == 0);
	}

	ds_test_assert(ds_async_queue_empty(queue));

	/* Test multiple poppers & pushers */
	for (i = 0; i < NUM_PUSHERS; i++) {
		ret = pthread_create(&threads[i], NULL, ds_async_queue_queue_push_thread, queue);
		ds_test_assert(ret == 0);
		ret = pthread_create(&threads[i + NUM_PUSHERS], NULL, ds_async_queue_queue_pop_thread, queue);
		ds_test_assert(ret == 0);
	}

	for (i = 0; i < NUM_PUSHERS; i++) {
		ret = pthread_join(threads[i], &status);
		ds_test_assert(ret == 0);
		ret = pthread_join(threads[i + NUM_PUSHERS], &status);
		ds_test_assert(ret == 0);
		ds_test_assert((long)status == NUM_PUSHS);
	}

	ds_test_assert(ds_async_queue_empty(queue));

	ds_async_queue_free(queue);

	return 0;
}

static int ds_append_buffer_test(void)
{
	struct ds_append_buffer abuf, abuf2;
	struct ds_append_buffer_iterator iter, iter2;
	unsigned char byte;
	unsigned int pos, i, buflen, fulllen;
	char buf[20];
	void *piece_buf;

	/* test append buffer initialization */
	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* free empty buffer */
	ds_append_buffer_free(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* fill into buffer */
	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_append_buffer_append(&abuf, "testing", sizeof("testing")) == sizeof("testing"));
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);

	/* free non-empty buffer */
	ds_append_buffer_free(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* move buffer */
	ds_append_buffer_init(&abuf);
	ds_append_buffer_append(&abuf, "testing", sizeof("testing"));
	ds_append_buffer_move(&abuf2, &abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);
	ds_test_assert(ds_append_buffer_length(&abuf2) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == false);
	ds_append_buffer_free(&abuf2);
	ds_test_assert(ds_append_buffer_length(&abuf2) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == true);

	/* clone buffer */
	ds_append_buffer_init(&abuf);
	ds_append_buffer_append(&abuf, "testing", sizeof("testing"));
	ds_append_buffer_clone(&abuf2, &abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);
	ds_test_assert(ds_append_buffer_length(&abuf2) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == false);
	ds_append_buffer_free(&abuf);
	ds_append_buffer_free(&abuf2);

	/* move head forward */
	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_append_buffer_append(&abuf, "testing", sizeof("testing")) == sizeof("testing"));
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);
	ds_test_assert(ds_append_buffer_move_head(&abuf, 0) == true);
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);
	ds_test_assert(ds_append_buffer_move_head(&abuf, 1) == true);
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("esting"));
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);

	/* clone buffer that has head moved */
	ds_append_buffer_clone(&abuf2, &abuf);
	ds_test_assert(ds_append_buffer_length(&abuf2) == sizeof("esting"));
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == false);

	/* free non-empty buffer that has head moved forward */
	ds_append_buffer_free(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* move head forward until buffer has been emptied */
	ds_test_assert(ds_append_buffer_move_head(&abuf2, 5) == true);
	ds_test_assert(ds_append_buffer_length(&abuf2) == sizeof("g"));
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == false);
	ds_test_assert(ds_append_buffer_move_head(&abuf2, 2) == true);
	ds_test_assert(ds_append_buffer_length(&abuf2) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf2.list) == true);
	ds_test_assert(ds_append_buffer_move_head(&abuf, 0) == true);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* move head forward too much (ds_append_buffer_move_head return false but empties buffer)*/
	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_append_buffer_append(&abuf, "testing", sizeof("testing")) == sizeof("testing"));
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_test_assert(ds_append_buffer_move_head(&abuf, 200) == false);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* piece_buf tests */
	piece_buf = ds_append_buffer_new_piece(&buflen);
	ds_test_assert(piece_buf != NULL);
	ds_append_buffer_free_piece(piece_buf);

	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);
	
	piece_buf = ds_append_buffer_new_piece(&buflen);
	fulllen = buflen;
	buflen = snprintf(piece_buf, buflen, "testing");
	ds_test_assert(piece_buf != NULL);

	ds_test_assert(ds_append_buffer_append_piece(&abuf, piece_buf, buflen));
	ds_test_assert(ds_append_buffer_length(&abuf) == buflen);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == false);

	i = ds_append_buffer_copy(&abuf, 0, buf, sizeof(buf));
	ds_test_assert(i == buflen);
	ds_test_assert(strncmp(buf, "testing", strlen("testing")) == 0);

	ds_append_buffer_move_head(&abuf, 1);
	i = ds_append_buffer_copy(&abuf, 0, buf, sizeof(buf));
	ds_test_assert(i == buflen - 1);
	ds_test_assert(strncmp(buf, "esting", strlen("esting")) == 0);

	piece_buf = ds_append_buffer_new_piece(&buflen);
	buflen = snprintf(piece_buf, buflen, "testing");
	ds_test_assert(!ds_append_buffer_append_piece(&abuf, piece_buf, buflen)); /* fails */
	ds_append_buffer_free_piece(piece_buf);

	piece_buf = ds_append_buffer_get_end_free(&abuf, &buflen);
	ds_test_assert(buflen + strlen("testing") == fulllen);
	buflen = snprintf(piece_buf, buflen, "testing");
	ds_test_assert(ds_append_buffer_length(&abuf) == strlen("esting"));
	ds_test_assert(ds_append_buffer_move_end(&abuf, buflen));

	i = ds_append_buffer_copy(&abuf, 0, buf, sizeof(buf));
	ds_test_assert(i == buflen * 2 - 1);
	ds_test_assert(strncmp(buf, "estingtesting", strlen("estingtesting")) == 0);

	ds_append_buffer_free(&abuf);

	/* Invalid move end */
	ds_append_buffer_init(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(!ds_append_buffer_move_end(&abuf, 1));
	ds_append_buffer_append(&abuf, buf, 1);
	ds_test_assert(ds_append_buffer_length(&abuf) == 1);
	ds_test_assert(ds_append_buffer_move_end(&abuf, fulllen - 2));
	ds_test_assert(ds_append_buffer_length(&abuf) == fulllen - 1);
	ds_test_assert(!ds_append_buffer_move_end(&abuf, 2));
	ds_test_assert(ds_append_buffer_move_end(&abuf, 1));
	ds_test_assert(ds_append_buffer_length(&abuf) == fulllen);
	ds_test_assert(!ds_append_buffer_move_end(&abuf, 1));
	ds_append_buffer_free(&abuf);

	/* Invalid get free */
	ds_append_buffer_init(&abuf);
	piece_buf = ds_append_buffer_get_end_free(&abuf, &buflen);
	ds_test_assert(piece_buf == NULL);
	ds_test_assert(buflen == 0);
	for (i = 0; i < fulllen; i++)
		ds_append_buffer_append(&abuf, buf, 1);
	piece_buf = ds_append_buffer_get_end_free(&abuf, &buflen);
	ds_test_assert(piece_buf == NULL);
	ds_test_assert(buflen == 0);
	ds_append_buffer_append(&abuf, buf, 1);
	piece_buf = ds_append_buffer_get_end_free(&abuf, &buflen);
	ds_test_assert(piece_buf != NULL);
	ds_test_assert(buflen == fulllen - 1);
	ds_append_buffer_free(&abuf);

	/* iterate empty buffer */
	ds_append_buffer_init(&abuf);
	ds_append_buffer_for_each(&iter, &abuf) {
		byte = ds_append_buffer_iterator_byte(&iter);
		pos = ds_append_buffer_iterator_pos(&iter);

		ds_test_assert(!"should not be called at all");
	}

	/* iterate filled buffer */
	ds_append_buffer_append(&abuf, "testing", sizeof("testing"));
	i = 0;
	ds_append_buffer_for_each(&iter, &abuf) {
		byte = ds_append_buffer_iterator_byte(&iter);
		pos = ds_append_buffer_iterator_pos(&iter);

		ds_test_assert(i == pos);
		ds_test_assert(byte == "testing"[pos]);

		i++;
	}
	ds_test_assert(ds_append_buffer_length(&abuf) == sizeof("testing"));
	ds_append_buffer_free(&abuf);
	ds_test_assert(ds_append_buffer_length(&abuf) == 0);
	ds_test_assert(ds_xorlist_empty(&abuf.list) == true);

	/* larger iteration test */
	ds_append_buffer_init(&abuf);
	for (i = 0; i < 1000*10; i++) {
		byte = i;
		ds_append_buffer_append(&abuf, &byte, 1);
	}
	i = 0;
	ds_append_buffer_for_each(&iter, &abuf) {
		byte = ds_append_buffer_iterator_byte(&iter);
		pos = ds_append_buffer_iterator_pos(&iter);

		ds_test_assert(i == pos);
		ds_test_assert(byte == (unsigned char)i);

		i++;
	}

	/* large clone and compare */
	ds_append_buffer_clone(&abuf2, &abuf);
	ds_append_buffer_iterator_init(&abuf2, &iter2);
	ds_append_buffer_for_each(&iter, &abuf) {
		ds_test_assert(!ds_append_buffer_iterator_has_reached_end(&iter2));

		byte = ds_append_buffer_iterator_byte(&iter);
		pos = ds_append_buffer_iterator_pos(&iter);

		ds_test_assert(byte == ds_append_buffer_iterator_byte(&iter2));
		ds_test_assert(pos == ds_append_buffer_iterator_pos(&iter2));

		ds_append_buffer_iterator_forward(&iter2, 1);
	}
	ds_append_buffer_free(&abuf);
	ds_append_buffer_free(&abuf2);

	/* larger iteration test with moved head */
	ds_append_buffer_init(&abuf);
	for (i = 0; i < 1000*10; i++) {
		byte = i;
		ds_append_buffer_append(&abuf, &byte, 1);
	}
	ds_append_buffer_move_head(&abuf, 9001);
	i = 0;
	ds_append_buffer_for_each(&iter, &abuf) {
		byte = ds_append_buffer_iterator_byte(&iter);
		pos = ds_append_buffer_iterator_pos(&iter);

		ds_test_assert(i == pos);
		ds_test_assert(byte == (unsigned char)(i + 9001));

		i++;
	}
	ds_append_buffer_free(&abuf);

	/* iterator test */
	ds_append_buffer_init(&abuf);
	ds_append_buffer_append(&abuf, "testing_0123456789", sizeof("testing_0123456789"));
	ds_append_buffer_iterator_init(&abuf, &iter);
	ds_append_buffer_iterator_forward(&iter, sizeof("testing"));
	ds_test_assert(ds_append_buffer_iterator_byte(&iter) == '0');
	ds_append_buffer_iterator_forward(&iter, 5);
	ds_test_assert(ds_append_buffer_iterator_byte(&iter) == '5');
	ds_append_buffer_iterator_forward(&iter, 1);
	ds_test_assert(ds_append_buffer_iterator_byte(&iter) == '6');
	ds_append_buffer_iterator_forward(&iter, 4);
	ds_test_assert(ds_append_buffer_iterator_byte(&iter) == '\0');
	ds_test_assert(!ds_append_buffer_iterator_has_reached_end(&iter));
	ds_append_buffer_iterator_forward(&iter, 1);
	ds_test_assert(ds_append_buffer_iterator_has_reached_end(&iter));
	ds_append_buffer_free(&abuf);

	/* copy test */
	ds_append_buffer_init(&abuf);
	for (i = 0; i < 100; i++)
		ds_test_assert(ds_append_buffer_append(&abuf, "testing_0123456789", sizeof("testing_0123456789")) == sizeof("testing_0123456789"));
	i = ds_append_buffer_copy(&abuf, sizeof("testing_0123456789") * 59 + sizeof("testing"), buf, sizeof(buf));
	ds_test_assert(strcmp(buf, "0123456789") == 0);
	ds_test_assert(i == 20);
	i = ds_append_buffer_copy(&abuf, sizeof("testing_0123456789") * 99 + sizeof("testing"), buf, sizeof(buf));
	ds_test_assert(strcmp(buf, "0123456789") == 0);
	ds_test_assert(i == sizeof("0123456789"));
	ds_append_buffer_free(&abuf);

	return 0;
}

static int run_test(const char *name, int (*test_fn)(void))
{
	int ret;
	
	printf("Testing \"%s\" ... ", name);
	fflush(stdout);
	fflush(stderr);
	
	if ((ret = test_fn()) != 0)
		goto out;
	
	printf("%s\n", "[OK]");
out:
	fflush(stdout);
	fflush(stderr);
	return ret;
}

int main(int argc, char *argv[])
{
	run_test("ds_basic", ds_basic_test);
	run_test("ds_linked_list", ds_linked_list_test);
	run_test("ds_xor_list", ds_xor_list_test);
	run_test("ds_append_buffer", ds_append_buffer_test);
	run_test("ds_queue", ds_queue_test);
	run_test("ds_async_queue", ds_async_queue_test);

	return 0;
}
