/*
 * Asynchronous message queue between threads.
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

#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#include "ds.h"

#define ASYNC_QUEUE_MAX_SIZE (128)

#ifndef DEBUG
	#define DISABLE_RUNTIME_TESTS
#endif

#define ds_assert(x)

struct ds_message {
	void *data;
	size_t data_length;
};

struct ds_async_queue {
	struct ds_linked_list message_list;

	pthread_mutex_t mutex;
	pthread_cond_t cond_wait_msg;
	pthread_cond_t cond_wait_free;
};

static void ds_async_queue_runtime_tests(struct ds_async_queue *queue);

/**
 * ds_alloc_message_list_entry - allocate message structure of requested size
 * @msglen: size of message data
 *
 * Allocates memory in such way that memory can be freed by using
 * pointer to message data. Simplifies handling data for ds_async_pop_*
 * caller as they can use regular free() for returned pointer.
 * Also avoids doing triple malloc for ds_list_entry + message + data.
 *
 * Memory structure will be:
 *  [message data (len: msglen + alignment)][struct ds_list_entry][struct ds_message]
 */
static struct ds_list_entry *ds_alloc_message_list_entry(size_t msglen)
{
	struct ds_list_entry *entry;
	size_t orig_len = msglen;
	unsigned char *mem;
	struct ds_message *message;

	/* Alloc message structure so that it can be passed by
	 * ds_async_queue_pop as regular malloced pointer.
	 */

	/* align length to long word size */
	if (msglen % sizeof(long) != 0)
		msglen = (msglen / sizeof(long) + 1) * sizeof(long);

	/* TODO: hard_malloc, tries to free resources, sleeps, etc on ENOMEM */
	mem = malloc(msglen + ds_sizeof_list_entry(struct ds_message));
	if (!mem)
		return NULL;

	entry = (void *)&mem[msglen];
	message = ds_list_entry_data_ptr(struct ds_message, entry);
	message->data_length = orig_len;
	message->data = mem;

	return entry;
}

static void ds_free_message(struct ds_message *msg)
{
	free(msg->data);
}

struct ds_async_queue *ds_async_queue_alloc(void)
{
	struct ds_async_queue *queue;
	int ret;

	queue = malloc(sizeof(*queue));
	if (!queue)
		return NULL;

	memset(queue, 0, sizeof(*queue));

	ds_list_init(&queue->message_list);

	ret = pthread_mutex_init(&queue->mutex, NULL);
	if (ret) {
		free(queue);
		return NULL;
	}

	ret = pthread_cond_init(&queue->cond_wait_msg, NULL);
	if (ret) {
		pthread_mutex_destroy(&queue->mutex);
		free(queue);
		return NULL;
	}

	ret = pthread_cond_init(&queue->cond_wait_free, NULL);
	if (ret) {
		pthread_cond_destroy(&queue->cond_wait_msg);
		pthread_mutex_destroy(&queue->mutex);
		free(queue);
		return NULL;
	}

	ds_async_queue_runtime_tests(queue);

	return queue;
}

void ds_async_queue_free(struct ds_async_queue *queue)
{
	pthread_mutex_lock(&queue->mutex);

	ds_list_purge(&queue->message_list, (void(*)(void *))ds_free_message);

	pthread_mutex_unlock(&queue->mutex);

	pthread_cond_destroy(&queue->cond_wait_free);
	pthread_cond_destroy(&queue->cond_wait_msg);
	pthread_mutex_destroy(&queue->mutex);

	free(queue);
}

bool ds_async_queue_empty(struct ds_async_queue *queue)
{
	bool empty;

	pthread_mutex_lock(&queue->mutex);
	empty = ds_list_empty(&queue->message_list);
	pthread_mutex_unlock(&queue->mutex);

	return empty;
}

static int pthread_cond_xwait(pthread_cond_t *cond, pthread_mutex_t *mutex,
			      const struct ds_timespec *abstime)
{
	struct timespec ts;

	if (!abstime)
		return pthread_cond_wait(cond, mutex);

	if (abstime->tv_sec == 0 && abstime->tv_nsec == 0)
		return ETIMEDOUT;

	ts.tv_sec = abstime->tv_sec;
	ts.tv_nsec = abstime->tv_nsec;
	return pthread_cond_timedwait(cond, mutex, &ts);
}

int ds_async_queue_push_timed(struct ds_async_queue *queue, void *msg,
			      size_t msglen, const struct ds_timespec *abstime)
{
	struct ds_list_entry *entry;
	size_t count;
	int ret;

	pthread_mutex_lock(&queue->mutex);

	/* check if size of message queue has grown too big */
	while ((count = ds_list_size(&queue->message_list)) >=
			ASYNC_QUEUE_MAX_SIZE) {
		ret = pthread_cond_xwait(&queue->cond_wait_free, &queue->mutex,
					 abstime);

		if (ret == ETIMEDOUT) {
			pthread_mutex_unlock(&queue->mutex);
			return -ETIMEDOUT;
		}
	}

	/* alloc and copy message */
	entry = ds_alloc_message_list_entry(msglen);
	/*
	 * TODO: hard_malloc, tries to free resources, sleeps and retries
	 * after ENOMEM
	 */
	if (!entry)
		return -ENOMEM;

	memmove(ds_list_entry_data_ptr(struct ds_message, entry)->data, msg,
		msglen);

	/* add new message to the list */
	ds_list_append_entry(&queue->message_list, entry);

	if (count == 0) {
		/* Signal about new message in the list.
		 *
		 * Note: we don't need to signal if list was not empty, as
		 * non-empty list means that none was currently waiting for
		 * messages.
		 */
		pthread_cond_broadcast(&queue->cond_wait_msg);
	}

	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int ds_async_queue_try_push(struct ds_async_queue *queue, void *msg,
			    size_t msglen)
{
	struct ds_timespec no_timeout = {0, 0};
	return ds_async_queue_push_timed(queue, msg, msglen, &no_timeout);
}

int ds_async_queue_pop_timed(struct ds_async_queue *queue, void **msg,
			     size_t *msglen, const struct ds_timespec *abstime)
{
	struct ds_list_entry *entry;
	struct ds_message *message;
	int ret;

	*msg = NULL;
	if (msglen)
		*msglen = 0;

	pthread_mutex_lock(&queue->mutex);

	while (ds_list_empty(&queue->message_list)) {
		ret = pthread_cond_xwait(&queue->cond_wait_msg, &queue->mutex,
					 abstime);

		if (ret == ETIMEDOUT) {
			pthread_mutex_unlock(&queue->mutex);
			return -ETIMEDOUT;
		}
	}

	entry = ds_list_first(&queue->message_list);
	message = ds_list_entry_data_ptr(struct ds_message, entry);

	*msg = message->data;
	if (msglen)
		*msglen = message->data_length;

	ds_list_remove_entry(&queue->message_list, entry);

	if (ds_list_size(&queue->message_list) == ASYNC_QUEUE_MAX_SIZE - 1) {
		/* Signal about list being small enough for new messages. */
		pthread_cond_broadcast(&queue->cond_wait_free);
	}

	pthread_mutex_unlock(&queue->mutex);

	return 0;
}

int ds_async_queue_try_pop(struct ds_async_queue *queue, void **msg,
			   size_t *msglen)
{
	struct ds_timespec no_timeout = {0, 0};
	return ds_async_queue_pop_timed(queue, msg, msglen, &no_timeout);
}

#ifndef DISABLE_RUNTIME_TESTS
static void ds_async_queue_runtime_tests(struct ds_async_queue *queue)
{
	struct ds_list_entry *entry;
	static bool tests_done = false;
	struct ds_message *message;
	char buf[11];
	char *popbuf;
	size_t popbuflen;
	int i, m;

	if (tests_done)
		return;
	tests_done = true;

	/* Test alloc/free message */
	entry = ds_alloc_message_list_entry(31);
	message = ds_list_entry_data_ptr(struct ds_message, entry);
	memset(message->data, 0xCC, 31);
	free(message->data);

	entry = ds_alloc_message_list_entry(32);
	message = ds_list_entry_data_ptr(struct ds_message, entry);
	memset(message->data, 0xCC, 32);
	free(message->data);

	entry = ds_alloc_message_list_entry(33);
	message = ds_list_entry_data_ptr(struct ds_message, entry);
	memset(message->data, 0xCC, 33);
	free(message->data);

	/* Test pushing and poping queue */
	memset(buf, 0x55, sizeof(buf));

	ds_async_queue_push(queue, buf, sizeof(buf));
	ds_async_queue_pop(queue, (void*)&popbuf, &popbuflen);

	free(popbuf);

	/* Test pushing 10 entries to queue and poping theim */
	m = ASYNC_QUEUE_MAX_SIZE < 10 ? ASYNC_QUEUE_MAX_SIZE : 10;
	for (i = 0; i < m; i++) {
		ds_async_queue_push(queue, buf, 1 + i);
	}

	ds_assert(ds_list_size(&queue->message_list) ==
		(ASYNC_QUEUE_MAX_SIZE < 10 ? ASYNC_QUEUE_MAX_SIZE : 10));

	for (i = 0; i < m; i++) {
		ds_async_queue_pop(queue, (void*)&popbuf, &popbuflen);

		ds_assert(popbuflen == 1 + i);

		free(popbuf);
	}
}
#else
static void ds_async_queue_runtime_tests(struct ds_async_queue *queue)
{
	/* dummy */
}
#endif
