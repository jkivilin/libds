/*
 * Simple data structures library
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

#ifndef __LIBDS__DS_H__
#define __LIBDS__DS_H__

#include <stdbool.h>
#include <stdint.h>


/*****************************************************************************
 * Very basic macros
 *****************************************************************************/
#define ds_offset_of(type, member) \
	((unsigned long)(&(((type *)0)->member)))

#define ds_container_of(ptr, type, member) \
	((type *)((unsigned long)(ptr) - ds_offset_of(type, member)))


/*****************************************************************************
 * Helper functions
 *****************************************************************************/

/*
 * helper struct to handle time consistently outside, internally
 * converted to system's 'struct timespec'.
 */
struct ds_timespec {
	long tv_nsec;
	long tv_sec;
};

/**
 * ds_get_curr_timespec - utility function for getting current system time
 * @ts: time data structure to store current time to
 */
extern int ds_get_curr_timespec(struct ds_timespec *ts);

/**
 * ds_add_timespec_usec - utility function for adding useconds to ds_timespec
 * @time: time structure
 * @usec: time in microseconds to add to @time
 */
extern void ds_add_timesec_usec(struct ds_timespec *time, unsigned int usec);

/**
 * ds_make_timeout_us - utility function for making absolute timeout from
 *			relative microsecond timeout
 * @abstimeout: absolute time of timeout
 * @timeout_us: relative timeout in microseconds
 */
extern int ds_make_timeout_us(struct ds_timespec *abstimeout,
			      unsigned int timeout_us);

/**
 * make_timeout_ms - utility function from making absolute timeout from relative
 *                   millisecond timeout
 * @abstimeout: absolute time of timeout
 * @timeout_ms: relative timeout in milliseconds
 */
extern int ds_make_timeout_ms(struct ds_timespec *abstimeout,
			      unsigned int timeout_ms);


/*****************************************************************************
 * Doubly linked list
 *****************************************************************************/
struct ds_list_entry {
	struct ds_list_entry *next, *prev;
};

struct ds_linked_list {
	struct ds_list_entry *head, *tail;
	int count;
};

/**
 * __ds_list_entry_data_ptr - helper function to hide strict aliasing warning
 * @entry: list entry
 */
static inline void *__ds_list_entry_data_ptr(void *entry)
{
	void *tmp = (char *)entry + sizeof(struct ds_list_entry);
	return tmp;
}

/**
 * ds_list_entry_data_ptr - return list entry data pointer in requested data
 *			    type
 * @type: type to cast data pointer to
 * @entry: list entry
 */
#define ds_list_entry_data_ptr(type, entry) \
	((type*)__ds_list_entry_data_ptr(entry))

/**
 * ds_list_entry_data - return list entry data in requested data type
 * @type: data type of list entry buffer
 * @entry: list entry
 */
#define ds_list_entry_data(type, entry) (*ds_list_entry_data_ptr(type, entry))

/**
 * sizeof_ds_list_entry - return size of list entry
 * @type: data type of list entry buffer
 */
#define ds_sizeof_list_entry(type) (sizeof(struct ds_list_entry) + sizeof(type))

/**
 * ds_set_list_entry_data - set list entry data buffer
 */
#define ds_set_list_entry_data(entry, type, data) \
		(*ds_list_entry_data_ptr(type, entry) = data)

/**
 * ds_list_entry_alloc - allocate memory area large enough for list_entry and
 *			 data
 * @datasize: size of list entry buffer
 */
extern struct ds_list_entry *ds_list_entry_alloc(size_t datasize);

/**
 * ds_new_list_entry - allocate list entry and set list entry data buffer
 * @entry: list entry variable to store list entry pointer
 * @type: data type of list entry buffer
 * @data: data for list entry buffer
 */
#define ds_new_list_entry(entry, type, data) do { \
		entry = ds_list_entry_alloc(sizeof(type)); \
		if (entry) \
			ds_set_list_entry_data(entry, type, data); \
	} while(false)

/**
 * ds_list_init - initialize given linked list structure
 * @list: linked list
 */
static inline void ds_list_init(struct ds_linked_list *list)
{
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

/**
 * ds_list_empty - checks if linked list is emptry
 * @list: linked list
 */
static inline bool ds_list_empty(struct ds_linked_list *list)
{
	return (list->count == 0);
}

/**
 * ds_list_size - returns number of entry in list
 * @list: linked list
 */
static inline unsigned int ds_list_size(struct ds_linked_list *list)
{
	if (list->count < 0)
		return 0;

	return list->count;
}

/**
 * ds_list_first - returns first entry in list
 * @list: linked list
 */
static inline struct ds_list_entry *ds_list_first(struct ds_linked_list *list)
{
	return list->head;
}

/**
 * ds_list_last - returns last entry in list
 * @list: linked list
 */
static inline struct ds_list_entry *ds_list_last(struct ds_linked_list *list)
{
	return list->tail;
}

/**
 * ds_list_append_entry - add new list entry element at end of list
 * @list: linked list
 * @entry: new list entry
 */
extern void ds_list_append_entry(struct ds_linked_list *list,
				 struct ds_list_entry *entry);

/**
 * ds_list_prepend_entry - add new list entry element at start of list
 * @list: linked list
 * @entry: new list entry
 */
extern void ds_list_prepend_entry(struct ds_linked_list *list,
				  struct ds_list_entry *entry);

/**
 * ds_list_append - add new data element at end of list
 * @list: linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
extern bool ds_list_append(struct ds_linked_list *list, void *data);

/**
 * ds_list_prepend - add new data element at start of list
 * @list: linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
extern bool ds_list_prepend(struct ds_linked_list *list, void *data);

/**
 * ds_list_find - find entry for data-pointer from list
 * @list: linked list
 * @data: data pointer to search list for
 */
extern struct ds_list_entry *ds_list_find(struct ds_linked_list *list,
					  const void *data);

/**
 * ds_list_remove_entry - remove entry from list
 * @list: linked list
 * @entry: entry of list to be removed
 *
 * Entry element is removed from list but not freed.
 */
extern void ds_list_remove_entry(struct ds_linked_list *list,
				 struct ds_list_entry *entry);

/**
 * ds_list_delete_entry - delete entry from list
 * @list: linked list
 * @entry: entry of list to be deleted
 *
 * Entry element is also freed.
 */
extern void ds_list_delete_entry(struct ds_linked_list *list,
				 struct ds_list_entry *entry);

/**
 * ds_list_purge - free list and all data elements
 * @list: linked list to be freed
 * @data_free: function callback used to free data elements. If NULL, data
 *             elements will not be freed.
 */
extern void ds_list_purge(struct ds_linked_list *list,
			  void (*data_free)(void *data));

/**
 * ds_list_free - free list but do not free data elements
 * @list: linked list to be freed
 */
static inline void ds_list_free(struct ds_linked_list *list)
{
	ds_list_purge(list, NULL);
}

/**
 * ds_list_for_each - for statement macro for iterating linked list forwards
 * @pos: list position entry
 * @list: linked list
 *
 * NOTE: You may not remove entries from list within this for-loop.
 */
#define ds_list_for_each(pos, list) \
	for ((pos) = (list)->head; (pos) != NULL; (pos) = (pos)->next)

/**
 * ds_list_for_each - for statement macro for iterating linked list backwards
 * @pos: list position entry
 * @list: linked list
 *
 * NOTE: You may not remove entries from list within this for-loop.
 */
#define ds_list_for_each_tail(pos, list) \
	for ((pos) = (list)->tail; (pos) != NULL; (pos) = (pos)->prev)


/*****************************************************************************
 * XOR linked list
 *****************************************************************************/
struct ds_xorlist_entry {
	uintptr_t prevnext;
};

struct ds_xor_list {
	struct ds_xorlist_entry *head, *tail;
	int count;
};

/**
 * __ds_xorlist_entry_data_ptr - helper function to hide strict aliasing warning
 * @entry: xorlist entry
 */
static inline void *__ds_xorlist_entry_data_ptr(void *entry)
{
	void *tmp = (char *)entry + sizeof(struct ds_xorlist_entry);
	return tmp;
}

/**
 * ds_xorlist_entry_data_ptr - return xorlist entry data pointer in requested
 *			       data type
 * @type: type to cast data pointer to
 * @entry: xorlist entry
 */
#define ds_xorlist_entry_data_ptr(type, entry) \
	((type*)__ds_xorlist_entry_data_ptr(entry))

/**
 * ds_xorlist_entry_data - return xorlist entry data in requested data type
 * @type: data type of xorlist entry buffer
 * @entry: list entry
 */
#define ds_xorlist_entry_data(type, entry) \
	(*ds_xorlist_entry_data_ptr(type, entry))

/**
 * sizeof_ds_xorlist_entry - return size of xorlist entry
 * @type: data type of xorlist entry buffer
 */
#define ds_sizeof_xorlist_entry(type) \
	(sizeof(struct ds_xorlist_entry) + sizeof(type))

/**
 * ds_set_xorlist_entry_data - set xorlist entry data buffer
 */
#define ds_set_xorlist_entry_data(entry, type, data) \
	(*ds_xorlist_entry_data_ptr(type, entry) = data)

/**
 * ds_xorlist_entry_alloc - allocate memory area large enough for xorlist_entry
 *			    and data
 * @datasize: size of xorlist entry buffer
 */
extern struct ds_xorlist_entry *ds_xorlist_entry_alloc(size_t datasize);

/**
 * ds_new_xorlist_entry - allocate xorlist entry and set xorlist entry data
 *			  buffer
 * @entry: xorlist entry variable to store xorlist entry pointer
 * @type: data type of xorlist entry buffer
 * @data: data for xorlist entry buffer
 */
#define ds_new_xorlist_entry(entry, type, data) do { \
		entry = ds_xorlist_entry_alloc(sizeof(type)); \
		if (entry) \
			ds_set_xorlist_entry_data(entry, type, data); \
	} while(false)

/**
 * ds_xorlist_init - initialize given xor linked list structure
 * @list: xor linked list
 */
static inline void ds_xorlist_init(struct ds_xor_list *list)
{
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

/**
 * ds_xorlist_empty - checks if xor linked list is emptry
 * @list: xor linked list
 */
static inline bool ds_xorlist_empty(struct ds_xor_list *list)
{
	return (list->count == 0);
}

/**
 * ds_xorlist_size - returns number of entry in xor list
 * @list: xor linked list
 */
static inline unsigned int ds_xorlist_size(struct ds_xor_list *list)
{
	if (list->count < 0)
		return 0;

	return list->count;
}

/**
 * ds_xorlist_first - returns first entry in xor list
 * @list: xor linked list
 */
static inline struct ds_xorlist_entry *
ds_xorlist_first(struct ds_xor_list *list)
{
	return list->head;
}

/**
 * ds_xorlist_last - returns last entry in xor list
 * @list: xor linked list
 */
static inline struct ds_xorlist_entry *ds_xorlist_last(struct ds_xor_list *list)
{
	return list->tail;
}

/**
 * ds_xorlist_append_entry - add new xorlist entry element at end of xor list
 * @list: xor linked list
 * @entry: new xor list entry
 */
extern void ds_xorlist_append_entry(struct ds_xor_list *list,
				    struct ds_xorlist_entry *entry);

/**
 * ds_xorlist_prepend_entry - add new xorlist entry element at start of xor list
 * @list: xor linked list
 * @entry: new xor list entry
 */
extern void ds_xorlist_prepend_entry(struct ds_xor_list *list,
				     struct ds_xorlist_entry *entry);

/**
 * ds_xorlist_append - add new data element at end of xor list
 * @list: xor linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
extern bool ds_xorlist_append(struct ds_xor_list *list, void *data);

/**
 * ds_xorlist_prepend - add new data element at start of xor list
 * @list: xor linked list
 * @data: new data element
 *
 * Returns false in case of running out-of-memory.
 */
extern bool ds_xorlist_prepend(struct ds_xor_list *list, void *data);

/**
 * ds_xorlist_find - find entry for data-pointer from xor list
 * @list: xor linked list
 * @data: data pointer to search list for
 */
extern struct ds_xorlist_entry *ds_xorlist_find(struct ds_xor_list *list,
						const void *data);

/**
 * ds_xorlist_remove_entry - remove entry from xor list
 * @list: xor linked list
 * @entry: entry of xor list to be removed
 *
 * Entry element is removed from list but not freed.
 */
extern void ds_xorlist_remove_entry(struct ds_xor_list *list,
				    struct ds_xorlist_entry *entry,
				    struct ds_xorlist_entry *prev);

/**
 * ds_list_delete_entry - delete entry from list
 * @list: linked list
 * @entry: entry of list to be deleted
 *
 * Entry element is also freed.
 */
extern void ds_xorlist_delete_entry(struct ds_xor_list *list,
				    struct ds_xorlist_entry *entry,
				    struct ds_xorlist_entry *prev);

/**
 * ds_xorlist_purge - free xor list and all data elements
 * @list: xor linked list to be freed
 * @data_free: function callback used to free data elements. If NULL, data
 *             elements will not be freed.
 */
extern void ds_xorlist_purge(struct ds_xor_list *list,
			     void (*data_free)(void *data));

/**
 * ds_xorlist_free - free xor list but do not free data elements
 * @list: xor linked list to be freed
 */
static inline void ds_xorlist_free(struct ds_xor_list *list)
{
	ds_xorlist_purge(list, NULL);
}

/**
 * ds_xorlist_next - get next xor list entry
 * @prev: previous xor list entry
 * @pos: current xor list entry
 */
#define ds_xorlist_next(prev, pos) \
	((struct ds_xorlist_entry *)((uintptr_t)(prev) ^ (pos)->prevnext))

/**
 * ds_xorlist_set_next - set next xor list entry to @pos and @pos to @prev
 * @prev: previous xor list entry
 * @pos: current xor list entry
 */
static inline void ds_xorlist_set_next(struct ds_xorlist_entry **prev,
				       struct ds_xorlist_entry **pos)
{
	struct ds_xorlist_entry *next;

	next = ds_xorlist_next(*prev, *pos);
	*prev = *pos;
	*pos = next;
}

/**
 * ds_xorlist_for_each - for statement macro for iterating xor linked list
 *			 forwards
 * @prev: xor list previous position entry
 * @pos: xor list position entry
 * @list: xor linked list
 *
 * NOTE: You may not remove entries from list within this for-loop.
 */
#define ds_xorlist_for_each(prev, pos, list) \
	for ((prev) = NULL, (pos) = (list)->head; (pos) != NULL; \
		ds_xorlist_set_next(&(prev), &(pos)))

/**
 * ds_xorlist_for_each - for statement macro for iterating xor linked list
 *			 backwards
 * @pos: xor list position entry
 * @list: xor linked list
 *
 * NOTE: You may not remove entries from list within this for-loop.
 */
#define ds_xorlist_for_each_tail(prev, pos, list) \
	for ((prev) = NULL, (pos) = (list)->tail; (pos) != NULL; \
		ds_xorlist_set_next(&(prev), &(pos)))


/*****************************************************************************
 * Queue, simple implementation by reusing ds_linked_list
 *****************************************************************************/
struct ds_queue {
	struct ds_linked_list q;
};

/**
 * ds_queue_init - initialize given queue structure
 * @q: queue
 */
static inline void ds_queue_init(struct ds_queue *q)
{
	ds_list_init(&q->q);
}

/**
 * ds_queue_size - return number of data elements in queue
 * @q: queue
 */
static inline unsigned int ds_queue_size(struct ds_queue *q)
{
	return ds_list_size(&q->q);
}

/**
 * ds_queue_push_entry - push list entry to begining of queue
 * @q: queue
 * @entry: list entry
 */
static inline void ds_queue_push_entry(struct ds_queue *q,
				       struct ds_list_entry *entry)
{
	ds_list_prepend_entry(&q->q, entry);
}

/**
 * ds_queue_push_ptr - push data pointer element to begining of queue
 * @q: queue
 * @data: data pointer
 */
static inline bool ds_queue_push_ptr(struct ds_queue *q, void *data)
{
	return ds_list_prepend(&q->q, data);
}

/**
 * ds_queue_push_data - push typed data element to begining of queue
 * @q: queue
 * @type: type of data
 * @data: data variable
 */
#define ds_queue_push_data(q, type, data) do { \
		struct ds_list_entry *__entry; \
		ds_new_list_entry(__entry, type, data); \
		if (__entry) \
			ds_queue_push_entry(q, __entry); \
	} while(false)

/**
 * ds_queue_peek_entry - peek last entry of queue but do not pop
 * @q: queue
 *
 * Returns NULL if queue is empty.
 */
static inline struct ds_list_entry *ds_queue_peek_entry(struct ds_queue *q)
{
	return ds_list_last(&q->q);
}

/**
 * ds_queue_peek - peek last data element of queue but do not pop
 * @q: queue
 *
 * Return NULL if queue is empty (or if data element was NULL).
 */
static inline void *ds_queue_peek(struct ds_queue *q)
{
	struct ds_list_entry *last = ds_queue_peek_entry(q);

	return last ? ds_list_entry_data(void *, last) : NULL;
}

/**
 * __ds_queue_peek_data - peek last data buffer of queue but do not pop
 * @q: queue
 * @buf: data buffer
 * @buflen: length of buffer
 *
 * Return false if queue is empty, true if data was copied over to buffer
 */
extern bool __ds_queue_peek_data(struct ds_queue *q, void *buf, size_t buflen);

/**
 * ds_queue_peek_data - peek last data type of queue but do not pop
 * @q: queue
 * @type: type of data
 * @data: data variable
 *
 */
#define ds_queue_peek_data(q, type, data) \
	(__ds_queue_peek_data(q, &(data), sizeof(type)), data)

/**
 * ds_queue_pop_entry - pop entry from end of queue
 * @q: queue
 *
 * Return NULL if queue is empty.
 */
extern struct ds_list_entry *ds_queue_pop_entry(struct ds_queue *q);

/**
 * ds_queue_pop_ptr - pop data pointer element from end of queue
 * @q: queue
 *
 * Return NULL if queue is empty (or if data element was NULL).
 */
extern void *ds_queue_pop_ptr(struct ds_queue *q);

/**
 * __ds_queue_pop_data - pop data buffer element from end of queue
 * @q: queue
 * @buf: data buffer
 * @buflen: length of buffer
 *
 * Return false if queue is empty, true if data was copied over to buffer
 */
extern bool __ds_queue_pop_data(struct ds_queue *q, void *buf, size_t buflen);

/**
 * ds_queue_pop_data - pop data type element from end of queue
 * @q: queue
 * @type: type of data
 * @data: data variable
 *
 * Return false if queue is empty, true if data was copied over to data variable
 */
#define ds_queue_pop_data(q, type, data) \
	(__ds_queue_pop_data(q, &(data), sizeof(type)))

/**
 * ds_queue_free - frees queue entries
 * @q: queue
 */
extern void ds_queue_free(struct ds_queue *q);

/**
 * ds_queue_for_each - for statement macro for iterating queue forwards
 * @pos: queue list position entry (struct list_entry)
 * @queue: queue
 *
 * NOTE: You may not remove entries from list within this for-loop.
 */
#define ds_queue_for_each(pos, queue) \
	ds_list_for_each_tail(pos, &((queue)->q))


/*****************************************************************************
 * Asynchronous message queue between threads
 *****************************************************************************/

/* private structure, defined in ds_async_queue.c */
struct ds_async_queue;

/**
 * ds_async_queue_alloc - create new ds_async_queue
 */
extern struct ds_async_queue *ds_async_queue_alloc(void);

/**
 * ds_async_queue_free - frees ds_async_queue
 * @queue: queue to free
 */
extern void ds_async_queue_free(struct ds_async_queue *queue);

/**
 * ds_async_queue_empty - checks if queue is empty
 * @queue: queue
 */
extern bool ds_async_queue_empty(struct ds_async_queue *queue);

/**
 * ds_async_queue_push_timed - pushes new message to queue, with timeout
 * @queue: queue to push message to
 * @msg: pointer to message data
 * @msglen: size of message data
 * @abstime: absolute time of timeout
 *
 * Pushes message to queue. If queue is full, will block until there is space
 * or timeout is reached. If timeout is reached, returns -ETIMEDOUT. Otherwise
 * returns zero. Caller retains ownership of @msg object and may free it.
 */
extern int ds_async_queue_push_timed(struct ds_async_queue *queue, void *msg,
				     size_t msglen,
				     const struct ds_timespec *abstime);

/**
 * ds_async_queue_try_push - tries to push new message to queue
 * @queue: queue to push message to
 * @msg: pointer to message data
 * @msglen: size of message data
 *
 * Tries to push message to queue. If queue is full, will return -ETIMEDOUT.
 * Otherwise returns zero. Caller retains ownership of @msg object and may
 * free it.
 */
extern int ds_async_queue_try_push(struct ds_async_queue *queue, void *msg,
				   size_t msglen);

/**
 * ds_async_queue_push - pushes new message to queue
 * @queue: queue to push message to
 * @msg: pointer to message data
 * @msglen: size of message data
 *
 * Pushes message to queue. If queue is full, will block until there is space.
 * Caller retains ownership of @msg object and may free it.
 */
static inline void ds_async_queue_push(struct ds_async_queue *queue, void *msg,
				       size_t msglen)
{
	ds_async_queue_push_timed(queue, msg, msglen, NULL);
}

/**
 * ds_async_queue_pop_timed - pops message from queue, with timeout
 * @queue: queue
 * @msg: returns pointer to message
 * @msglen: returns size of message
 * @abstime: absolute time of timeout
 *
 * Pops message from queue. If queue is empty, will block until there is message
 * or timeout is reached. If timeout is reached, returns -ETIMEDOUT. Otherwise
 * returns zero. Caller must after use free data structure returned by @msg.
 */
extern int ds_async_queue_pop_timed(struct ds_async_queue *queue, void **msg,
				    size_t *msglen,
				    const struct ds_timespec *abstime);

/**
 * ds_async_queue_try_pop - tries to pop message from queue
 * @queue: queue
 * @msg: returns pointer to message
 * @msglen: returns size of message
 *
 * Tries to pops message from queue. If queue is empty, returns -ETIMEDOUT.
 * Otherwise returns zero. Caller must after use free data structure returned
 * by @msg.
 */
extern int ds_async_queue_try_pop(struct ds_async_queue *queue, void **msg,
				  size_t *msglen);

/**
 * ds_async_queue_pop - pop message from queue
 * @queue: queue
 * @msg: returns pointer to message
 * @msglen: returns size of message
 *
 * Pops message from queue. If queue is empty, will block until there is
 * message. Caller must after use free data structure returned by @msg.
 */
static inline void ds_async_queue_pop(struct ds_async_queue *queue, void **msg,
				      size_t *msglen)
{
	ds_async_queue_pop_timed(queue, msg, msglen, NULL);
}


/*****************************************************************************
 * Appendable buffer, scatter/gather memory buffer with ability to append new
 * data.
 *****************************************************************************/

struct ds_append_buffer {
	struct ds_linked_list list;
	unsigned int length;
	unsigned int first_offset;
};

/**
 * ds_append_buffer_init - initialize appendable buffer
 * @buf: buffer structure to initialize
 */
static inline void ds_append_buffer_init(struct ds_append_buffer *buf)
{
	ds_list_init(&buf->list);
	buf->length = 0;
	buf->first_offset = 0;
}

/**
 * ds_append_buffer_length - get length of data stored to buffer
 * @buf: appendable buffer
 */
static inline unsigned int ds_append_buffer_length(struct ds_append_buffer *buf)
{
	return buf->length;
}

/**
 * ds_append_buffer_free - free internal data structures and data of buffer
 * @buf: buffer to free
 */
extern void ds_append_buffer_free(struct ds_append_buffer *buf);

/**
 * ds_append_buffer_move - move internal data structures from one buffer to
 *			   another, also clears the old buffer.
 * @new_buf: buffer which data is moved to
 * @old_buf: buffer where data is copied from, and buffer which is
 *           cleared/reinitialized
 */
extern void ds_append_buffer_move(struct ds_append_buffer *new_buf,
				  struct ds_append_buffer *old_buf);

/**
 * ds_append_buffer_clone - makes copy of @old_buf to @new_buf.
 * @new_buf: empty buffer
 * @old_buf: buffer to be cloned
 *
 * Return false in case of memory allocation failure. Otherwise true.
 */
extern bool ds_append_buffer_clone(struct ds_append_buffer *new_buf,
				      const struct ds_append_buffer *old_buf);

/**
 * ds_append_buffer_append - appends data from @inbuf at end of data in @abuf
 * @abuf: appendable buffer
 * @inbuf: input buffer with new data
 * @len: length of @inbuf
 *
 * Returns number of bytes copied, should always be @len except in case of
 * memory allocation failure.
 */
extern unsigned int ds_append_buffer_append(struct ds_append_buffer *abuf,
					    const void *inbuf,
					    unsigned int len);

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
extern unsigned int ds_append_buffer_copy(struct ds_append_buffer *abuf,
					  unsigned int offset, void *buf,
					  unsigned int len);

/**
 * ds_append_buffer_move_head - moves head of buffer forward and frees memory
 *                              left unused
 * @abuf: appendable buffer
 * @add: how many bytes to move head forward
 */
extern bool ds_append_buffer_move_head(struct ds_append_buffer *abuf,
				       unsigned int add);


/**
 * ds_append_buffer_get_end_free - Get unused memory at end of append_buffer
 * @abuf: appendable buffer
 * @freelen: length of returned buffer
 */
extern void *ds_append_buffer_get_end_free(struct ds_append_buffer *abuf,
					   unsigned int *freelen);

/**
 * ds_append_buffer_move_end - moves end of buffer forward (marks unused
 *			       trailing buffer as in use)
 * @abuf: appendable buffer
 * @add: how many bytes to move end forward
 */
extern bool ds_append_buffer_move_end(struct ds_append_buffer *abuf,
				      unsigned int add);

/**
 * ds_append_buffer_new_piece - allocate new internal data buffer and return
 *                              pointer to it's data array.
 * @buflen: number of bytes in data array is stored here
 */
extern void *ds_append_buffer_new_piece(unsigned int *buflen);

/**
 * ds_append_buffer_free_piece - free unused piece that was allocated
 * @piece_buf: pointer to data array of piece
 */
extern void ds_append_buffer_free_piece(void *piece);

/**
 * ds_append_buffer_get_write_buffer - get direct-write-buffer that will
 *				       result writing directly to appendable
 *				       buffer
 * @abuf: appendable buffer
 * @buflen: length of returned write buffer
 */
extern void *ds_append_buffer_get_write_buffer(struct ds_append_buffer *abuf,
					       unsigned int *buflen);

/**
 * ds_append_buffer_finish_write_buffer - finish direct-write-buffer (either
 *					  append or free)
 * @abuf: appendable buffer
 * @write_buf: write buffer where caller has written new appended bytes
 * @buflen: number of used bytes in write buffer
 */
extern bool ds_append_buffer_finish_write_buffer(struct ds_append_buffer *abuf,
						 void *write_buf,
						 unsigned int buflen);

/**
 * ds_append_buffer_append_piece - append piece_buf at end of appendable buffer.
 * 				   NOTE: appendable buffer must not have
 *				   trailing unused bytes when calling this
 *				   function.
 * 				   Use ds_append_buffer_get_end_free() to check
 * 				   that number of unused bytes is zero.
 * @abuf: appendable buffer
 * @piece_buf: buffer to add, must be allocated by using
 *	       ds_append_buffer_new_piece()
 * @buflen: bytes in use in piece_buf
 */
extern bool ds_append_buffer_append_piece(struct ds_append_buffer *abuf,
					  void *piece, unsigned int buflen);

/**
 * ds_append_buffer_iterator - appendable buffer iterator helper structure
 *
 * Do not use directly, unstable/internal data structure.
 */
struct ds_append_buffer_iterator {
	unsigned char *pchar;
	unsigned int ppos;
	unsigned int pos;
	unsigned int pmax;
};

/**
 * ds_append_buffer_iterator_init - initialize appendable buffer iterator
 */
extern void
ds_append_buffer_iterator_init(struct ds_append_buffer *abuf,
			       struct ds_append_buffer_iterator *iter);

/**
 * __ds_append_buffer_iterator_forward - slow path for moving iterator forward
 *					 by @add bytes
 * @iter: iterator structure
 * @add: number of bytes to move iterator forward
 */
extern void
__ds_append_buffer_iterator_forward(struct ds_append_buffer_iterator *iter,
				    unsigned int add);

/**
 * ds_append_buffer_iterator_forward - fast path for moving iterator forward
 *				       by @add bytes
 * @iter: iterator structure
 * @add: number of bytes to move iterator forward
 */
static inline void
ds_append_buffer_iterator_forward(struct ds_append_buffer_iterator *iter,
				  unsigned int add)
{
	if (!iter->pchar || add == 0)
		return;

	/* do slow path if need to update pchar pointer to new piece */
	if (iter->ppos + add >= iter->pmax) {
		__ds_append_buffer_iterator_forward(iter, add);
		return;
	}

	/* Update iterator structure with new position */
	iter->ppos += add;
	iter->pos += add;
	iter->pchar += add;
}

/**
 * ds_append_buffer_iterator_has_reached_end - check if reached end of
 *					       appendable buffer
 * @iter: iterator structure
 */
static inline bool ds_append_buffer_iterator_has_reached_end(
					struct ds_append_buffer_iterator *iter)
{
	return (!iter->pchar);
}

/**
 * ds_append_buffer_iterator_byte - get current iterator byte
 * @iter: iterator
 *
 * ds_append_buffer_iterator_has_reached_end() must be checked before calling
 * this function and must not be called if iterator has reached the end!
 */
static inline unsigned char ds_append_buffer_iterator_byte(
					struct ds_append_buffer_iterator *iter)
{
	return *iter->pchar;
}

/**
 * ds_append_buffer_iterator_pos - get current iterator position
 * @iter: iterator
 *
 * ds_append_buffer_iterator_has_reached_end() must be checked before calling
 * this function and must not be called if iterator has reached the end!
 */
static inline unsigned int ds_append_buffer_iterator_pos(
					struct ds_append_buffer_iterator *iter)
{
	/*
	 * ds_append_buffer_iterator_has_reached_end() must be checked before
	 * this.
	 */
	return iter->pos;
}

/**
 * ds_queue_for_each - for statement macro for iterating appendable buffer
 *		       byte data
 * @iter: iterator structure to be used, will be reseted
 * @abuf: appendable buffer to be iterated
 */
#define ds_append_buffer_for_each(iter, abuf) \
	for (ds_append_buffer_iterator_init(abuf, iter); \
		!ds_append_buffer_iterator_has_reached_end(iter); \
			ds_append_buffer_iterator_forward(iter, 1))

#endif /* __LIBDS__DS_H__ */
