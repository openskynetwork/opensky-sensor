#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <buffer.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <statistics.h>
#include <cfgfile.h>
#include <threads.h>
#include <util.h>

/** Define to 1 to enable debugging messages */
#define BUF_DEBUG 1
/** Define to 1 for general asserts, 2 for list checks and
 *   3 for exhaustive checks */
#define BUF_ASSERT 2

/* Forward declaration */
struct Pool;

/** Linked Message List */
struct MsgLink {
	/** Message */
	struct MSG_Message message;
	/** Next Element */
	struct MsgLink * next;
	/** Previous Element */
	struct MsgLink * prev;
	/** Next Element in typed queue */
	struct MsgLink * qnext;
	/** Containing pool */
	struct Pool * pool;
};

/** Linked List Container */
struct MsgList {
	/** Head Pointer or NULL if empty */
	struct MsgLink * head;
	/** Tail Pointer or NULL if empty */
	struct MsgLink * tail;
	/** Size of list */
	size_t size;
};

/** Buffer Pool */
struct Pool {
	/** Pool start */
	struct MsgLink * pool;
	/** Pool size */
	size_t size;
	/** Linked List: next pool */
	struct Pool * next;
	/** Garbage collection: collected messages */
	struct MsgList collect;
};

/** Static Pool (those messages are always allocated in advance) */
static struct Pool staticPool;

/** Dynamic Pool max. increments */
static size_t dynMaxIncrements;
/** Dynamic Pool increments */
static size_t dynIncrements;
/** Dynamic Pool List */
static struct Pool * dynPools;

/** Number of messages discarded in an overload situation */
static uint64_t overload;
static uint64_t overloadMax;
static bool inOverload;

/** Overall Pool */
static struct MsgList pool;
/** Output Queue */
static struct MsgList queue;
/** Output Queues (Typed) */
static struct MsgList tqueues[MSG_TYPES];

/** Currently processed message (by reader), for debugging purposes */
static struct MsgLink * currentMessage;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Consumer condition */
static pthread_cond_t queuecond = PTHREAD_COND_INITIALIZER;
/** Producer condition */
static pthread_cond_t poolcond = PTHREAD_COND_INITIALIZER;

static void construct();
static void destruct();
static void mainloop();
static bool start(struct Component * c, void * data);
static void stop(struct Component * c);

static void cleanup(void * dummy);

struct Component BUF_comp = {
	.description = "BUF",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop,
	.start = &start,
	.stop = &stop
};

static bool deployPool(struct Pool * newPool, size_t size);
static bool createDynPool();
static void collectPools();
static bool uncollectPools();
static void destroyUnusedPools();

static inline struct MsgLink * shift(struct MsgList * list);
static inline void unshift(struct MsgList * list, struct MsgLink * msg);
static inline void push(struct MsgList * list,
	struct MsgLink * msg);
static inline void clear(struct MsgList * list);

static inline void appendPool(const struct MsgList * newPool);

static inline struct MsgLink * shiftQueue();
static inline struct MsgLink * shiftTQueue(struct MsgList * list);
static inline void unshiftQueue(struct MsgLink * msg);
static inline void pushQueue(struct MsgLink * msg);
static inline void clearQueue();

#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
static void checkLists();
#else
#define checkLists() do {} while(0)
#endif

/** Initialize message buffer. */
static void construct()
{
	dynIncrements = 0;

	dynPools = NULL;
	clear(&pool);
	clearQueue();

	checkLists();

	currentMessage = NULL;

	dynMaxIncrements = CFG_config.buf.history ?
		CFG_config.buf.dynIncrement : 0;

	if (!deployPool(&staticPool, CFG_config.buf.statBacklog))
		error(-1, errno, "malloc failed");
}

static void destruct()
{
	struct Pool * dyn, * nextDyn;
	for (dyn = dynPools; dyn; dyn = nextDyn) {
		nextDyn = dyn->next;
		free(dyn->pool);
		free(dyn);
	}
	free(staticPool.pool);
}

static bool start(struct Component * c, void * data)
{
	if (CFG_config.buf.gcEnabled)
		return COMP_startThreaded(c, data);
	return true;
}

static void stop(struct Component * c)
{
	if (CFG_config.buf.gcEnabled)
		COMP_stopThreaded(c);
}

/** Gargabe collector mainloop.
 * \note intialize garbage collection first using BUF_initGC */
static void mainloop()
{
	while (true) {
		sleep(CFG_config.buf.gcInterval);
		pthread_mutex_lock(&mutex);
		if (!inOverload && queue.size <
			(dynIncrements * CFG_config.buf.dynBacklog) /
				CFG_config.buf.gcLevel) {
			++STAT_stats.BUF_GCRuns;
#ifdef BUF_DEBUG
			NOC_puts("BUF: Running Garbage Collector");
#endif
			collectPools();
			destroyUnusedPools();
			checkLists();
		}
		pthread_mutex_unlock(&mutex);
	}
}

/** Flush Buffer queue. This discards all buffered but not processed messages */
void BUF_flush()
{
	pthread_mutex_lock(&mutex);
	appendPool(&queue);
	clearQueue();
	checkLists();
	pthread_mutex_unlock(&mutex);
	++STAT_stats.BUF_flushes;
}

/** Fill missing statistics */
void BUF_fillStatistics()
{
	STAT_stats.BUF_pools = dynIncrements;
	STAT_stats.BUF_queue = queue.size;
	STAT_stats.BUF_pool = pool.size;
	STAT_stats.BUF_sacrificeMax = overloadMax;
	STAT_stats.BUF_overload = inOverload;
}

static inline void overrun()
{
	if (overload == 0) {
		NOC_puts("BUF: running in overload mode -> sacrificing messages");
		inOverload = true;
	}
	if (++overload > overloadMax)
		overloadMax = overload;
}

static inline void overrunEnd()
{
	if (overload != 0) {
		NOC_printf("BUF: running in normal mode again. Sacrificed %" PRIu64
			" messages.\n", overload);
		overload = 0;
		inOverload = false;
	}
}

/** Get a new message from the pool. Extend the pool if there are more dynamic
 *  pools to get. Discard oldest message if there is no more dynamic pool
 * \return a message which can be filled
 */
static struct MsgLink * getMessageFromPool(enum MSG_TYPE type)
{
	struct MsgLink * ret;

	if (pool.head) {
		/* pool is not empty -> unlink and return first message */
		ret = shift(&pool);
		overrunEnd();
	} else if (uncollectPools()) {
		/* pool was empty, but we could uncollect a pool which was about to be
		 *  garbage collected */
#ifdef BUF_DEBUG
		NOC_puts("BUF: Uncollected pool");
#endif
		++STAT_stats.BUF_uncollects;
		overrunEnd();
		ret = shift(&pool);
	} else if (dynIncrements < dynMaxIncrements && createDynPool()) {
		/* pool was empty, but we just created another one */
#ifdef BUF_DEBUG
		NOC_printf("BUF: Created another pool (%zu/%zu)\n", dynIncrements,
			dynMaxIncrements);
#endif
		overrunEnd();
		ret = shift(&pool);
	} else {
		/* no more space in the pool and no more pools
		 * -> sacrifice oldest message */
		struct MsgList * tqueue = tqueues + MSG_TYPES - 1;
		const struct MsgList * end = tqueues + type;
		while (tqueue >= end && (ret = shiftTQueue(tqueue)) == NULL)
			--tqueue;

		if (ret) {
			overrun();
			ptrdiff_t qindex = tqueue - tqueues;
			++STAT_stats.BUF_sacrifices[qindex];
#if defined(BUF_DEBUG) && BUF_DEBUG > 1
			NOC_printf("BUF: sacrificing oldest %s for %s\n",
				MSG_TYPE_NAMES[qindex], MSG_TYPE_NAMES[type]);
#endif
		}
	}

	checkLists();
	return ret;
}

/** Get a message from the unused message pool for the writer in order to fill
 *  it.
 * \return new message
 */
struct MSG_Message * BUF_newMessage(enum MSG_TYPE type)
{
	pthread_mutex_lock(&mutex);
	struct MsgLink * link = getMessageFromPool(type);
	pthread_mutex_unlock(&mutex);
	if (link) {
#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
		assert (link->message.type == MSG_INVALID);
#endif
		link->message.type = type;
		return &link->message;
	} else {
		return NULL;
	}
}

/** Get a message from the unused message pool for the writer in order to fill
 *  it.
 * \return new message
 */
struct MSG_Message * BUF_newMessageWait(enum MSG_TYPE type)
{
	pthread_mutex_lock(&mutex);
	struct MsgLink * link;
	CLEANUP_PUSH(&cleanup, NULL);
	while (!(link = getMessageFromPool(type)))
		pthread_cond_wait(&poolcond, &mutex);
	CLEANUP_POP();
#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
	assert (link->message.type == MSG_INVALID);
#endif
	link->message.type = type;
	return &link->message;
}

/** Commit a filled message from writer to reader queue.
 * \param msg filled message to be delivered to the reader
 */
void BUF_commitMessage(struct MSG_Message * msg)
{
	assert (msg);
	struct MsgLink * link = container_of(msg, struct MsgLink, message);

#ifdef BUF_ASSERT
	assert (link->message.type < MSG_TYPES);
#endif
	pthread_mutex_lock(&mutex);
	pushQueue(link);
	checkLists();
	pthread_cond_broadcast(&queuecond);
	pthread_mutex_unlock(&mutex);

	if (queue.size > STAT_stats.BUF_maxQueue)
		STAT_stats.BUF_maxQueue = queue.size;
}

/** Abort filling a message and return it to the pool.
 * \param msg aborted message
 */
void BUF_abortMessage(struct MSG_Message * msg)
{
	assert (msg);
	struct MsgLink * link = container_of(msg, struct MsgLink, message);

#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
	link->message.type = MSG_INVALID;
#endif
	pthread_mutex_lock(&mutex);
	push(&pool, link);
	checkLists();
	pthread_cond_broadcast(&poolcond);
	pthread_mutex_unlock(&mutex);
}

static void cleanup(void * dummy)
{
	pthread_mutex_unlock(&mutex);
}

/** Get a message from the queue.
 * \return message
 */
const struct MSG_Message * BUF_getMessage()
{
#ifdef BUF_ASSERT
	assert (!currentMessage);
#endif
	CLEANUP_PUSH(&cleanup, NULL);
	pthread_mutex_lock(&mutex);
	while (!queue.head) {
		int r = pthread_cond_wait(&queuecond, &mutex);
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	}
	currentMessage = shiftQueue();
	checkLists();
	CLEANUP_POP();

#ifdef BUF_ASSERT
	assert (currentMessage->message.type < MSG_TYPES);
#endif
	return &currentMessage->message;
}

/** Get a message from the queue.
 * \param timeout_ms timeout in milliseconds
 * \return message or NULL on timeout
 */
const struct MSG_Message * BUF_getMessageTimeout(uint32_t timeout_ms)
{
	struct timespec ts;
	struct MSG_Message * ret;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		++ts.tv_sec;
		ts.tv_nsec -= 1000000000;
	}

#ifdef BUF_ASSERT
	assert (!currentMessage);
#endif

	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	while (!queue.head) {
		int r = pthread_cond_timedwait(&queuecond, &mutex, &ts);
		if (r == ETIMEDOUT) {
			ret = NULL;
			break;
		} else if (r) {
			error(-1, r, "pthread_cond_timedwait failed");
		}
	}
	if (queue.head) {
		currentMessage = shiftQueue();
#ifdef BUF_ASSERT
	assert (currentMessage->message.type < MSG_TYPES);
#endif
		ret = &currentMessage->message;
	}
	checkLists();
	CLEANUP_POP();

	return ret;
}

/** Put a message back into the pool.
 * \note Can be called by the reader after a message is processed in order to
 *  put the message into the pool again.
 * \param msg message to be put, must be the last one returned by one of
 *  BUF_getMessage() or BUF_getMessageTimeout()
 */
void BUF_releaseMessage(const struct MSG_Message * msg)
{
	assert (msg);
	assert (msg == &currentMessage->message);

#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
	currentMessage->message.type = MSG_INVALID;
#endif
	pthread_mutex_lock(&mutex);
	push(&pool, currentMessage);
	checkLists();
	pthread_cond_broadcast(&poolcond);
	pthread_mutex_unlock(&mutex);
	currentMessage = NULL;
}

/** Put a message back into the queue.
 * \note This is meant for unsuccessful behavior of the reader. It should be
 *  called if the reader wants to get that message again next time when
 *  BUF_getMessage() or BUF_getMessageTimeout() is called.
 * \param msg message to be put, must be the last one returned by one of
 *  BUF_getMessage() or BUF_getMessageTimeout()
 */
void BUF_putMessage(const struct MSG_Message * msg)
{
	assert (msg);
	assert (msg == &currentMessage->message);

#ifdef BUF_ASSERT
	assert (msg->type < MSG_TYPES);
#endif
	pthread_mutex_lock(&mutex);
	unshiftQueue(currentMessage);
	checkLists();
	pthread_cond_broadcast(&poolcond);
	pthread_mutex_unlock(&mutex);
	currentMessage = NULL;
}

/** Deploy a new pool: allocate messages and append them to the overall pool.
 * \param newPool pool to be deployed
 * \param size number of messages in that pool
 * \return true if deployment succeeded, false if allocation failed
 */
static bool deployPool(struct Pool * newPool, size_t size)
{
	/* allocate new messages */
	struct MsgLink * initMsgs = malloc(size * sizeof *initMsgs);
	if (!initMsgs)
		return false;

	/* initialize pool */
	newPool->pool = initMsgs;
	newPool->size = size;
	newPool->next = NULL;
	clear(&newPool->collect);

	/* setup message list */
	struct MsgList list;
	list.head = initMsgs;
	list.tail = initMsgs + size - 1;
	size_t i;
	struct MsgLink * msg = initMsgs;
	for (i = 0; i < size; ++i, ++msg) {
		msg->next = msg + 1;
		msg->prev = msg - 1;
		msg->pool = newPool;
	}
	list.head->prev = list.tail->next = NULL;
	list.size = size;

	/* append messages to the overall pool */
	appendPool(&list);
	checkLists();

	return true;
}

/** Create a new pool, deploy it and add it to the list of pools.
 * \return true if operation succeeded, false if allocation failed
 */
static bool createDynPool()
{
	struct Pool * newPool = malloc(sizeof *newPool);
	if (!newPool)
		return false;

	/* deployment */
	if (!deployPool(newPool, CFG_config.buf.dynBacklog)) {
		free(newPool);
		return false;
	}

	/* add pool to the list of dynamic pools */
	newPool->next = dynPools;
	dynPools = newPool;
	++dynIncrements;

	++STAT_stats.BUF_poolsCreated;
	if (dynIncrements > STAT_stats.BUF_maxPools)
		STAT_stats.BUF_maxPools = dynIncrements;

	return true;
}

/** Collect unused messages from the overall pool to their own pool.
 * \note used for garbage collection
 */
static void collectPools()
{
	struct MsgLink * msg, * next;
#ifdef BUF_DEBUG
	const size_t prevSize = pool.size;
#endif

	/* iterate over all messages of the pool and keep a pointer to the
	 * predecessor */
	for (msg = pool.head; msg; msg = next) {
		next = msg->next;
		if (msg->pool != &staticPool) {
			/* collect only messages of dynamic pools */

			/* unlink from the overall pool */
			if (msg->prev)
				msg->prev->next = next;
			else
				pool.head = next;
			if (next)
				next->prev = msg->prev;
			else
				pool.tail = msg->prev;
			--pool.size;

			/* add them to their pools' collection */
			unshift(&msg->pool->collect, msg);
		}
	}
#ifdef BUF_DEBUG
	NOC_printf("BUF: Collected %zu of %zu messages from overall pool to their "
		"dynamic pools\n", prevSize - pool.size, prevSize);
#endif
}

/** Revert effects of the garbage collection.
 * \return true if a dynamic pool was uncollected
 *  (i.e. there are new elements in the overall pool)
 */
static bool uncollectPools()
{
	struct Pool * p;

	/* iterate over all dynamic pools */
	for (p = dynPools; p; p = p->next) {
		if (p->collect.head) {
			/* pools' collection was not empty, put messages back into the
			 * overall pool */
			appendPool(&p->collect);
			/* clear collection */
			clear(&p->collect);
			return true;
		}
	}
	/* no such pool found */
	return false;
}

/** Free all fully collected pools.
 * \note used for garbage collection
 */
static void destroyUnusedPools()
{
	struct Pool * pool, * prev = NULL, * next;
	const size_t prevSize = dynIncrements;

	/* iterate over all dynamic pools */
	for (pool = dynPools; pool; pool = next) {
		next = pool->next;
		if (pool->collect.size == pool->size) {
			/* fully collected pool */

			/* unlink from the list of pools */
			if (prev)
				prev->next = next;
			else
				dynPools = next;
			--dynIncrements;

			/* delete messages */
			free(pool->pool);
			/* delete pool itself */
			free(pool);
		} else {
			prev = pool;
		}
	}
#ifdef BUF_DEBUG
	NOC_printf("BUF: Destroyed %zu of %zu dynamic pools\n",
		prevSize - dynIncrements, prevSize);
#endif
}

/** Unqueue the first element of a list and return it.
 * \param list list container
 * \return first element of the list or NULL if list was empty
 */
static inline struct MsgLink * shift(struct MsgList * list)
{
#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif

	if (!list->head) /* empty list */
		return NULL;

	/* first element */
	struct MsgLink * ret = list->head;

	/* unlink */
	list->head = ret->next;
	if (list->head)
		list->head->prev = NULL;
	else
		list->tail = NULL;
	ret->next = NULL;
	--list->size;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif
	return ret;
}

/** Enqueue a new message at front of a list.
 * \param list list container
 * \param msg the message to be enqueued
 */
static inline void unshift(struct MsgList * list, struct MsgLink * msg)
{
#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif

	/* message will be the new head */
	msg->next = list->head;
	msg->prev = NULL;

	/* link into container */
	if (!list->tail)
		list->tail = msg;
	else
		list->head->prev = msg;
	list->head = msg;
	++list->size;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif
}

/** Enqueue a new message at the end of a list.
 * \param list list container
 * \param msg message to be appended to the list
 */
static inline void push(struct MsgList * list, struct MsgLink * msg)
{
#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif

	/* message will be new tail */
	msg->next = NULL;
	msg->prev = list->tail;

	/* link into container */
	if (!list->head)
		list->head = msg;
	else
		list->tail->next = msg;
	list->tail = msg;
	++list->size;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
#endif
}

/** Append a new message list at the end of a list.
 * \param dstList destination list container
 * \param srcList source list container to be appended to the first list
 */
static inline void appendPool(const struct MsgList * newPool)
{
#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!pool.head == !!pool.tail);
	assert(!!newPool->head == !!newPool->tail);
#endif

#if defined BUF_ASSERT && BUF_ASSERT >= 3
	struct MsgLink * msg;
	for (msg = newPool->head; msg; msg = msg->next) {
		msg->message.type = MSG_INVALID;
		msg->qnext = NULL;
	}
#endif

	if (!pool.head) /* link head if dst is empty */
		pool.head = newPool->head;
	else /* link last element otherwise */
		pool.tail->next = newPool->head;
	if (newPool->tail) {
		/* link tail */
		newPool->head->prev = pool.tail;
		pool.tail = newPool->tail;
	}
	pool.size += newPool->size;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!pool.head == !!pool.tail);
#endif
}

static inline void clear(struct MsgList * list)
{
	list->head = list->tail = NULL;
	list->size = 0;
}

/** Unqueue the first element of a list and return it.
 * \param list list container
 * \return first element of the list or NULL if list was empty
 */
static inline struct MsgLink * shiftQueue()
{
	struct MsgLink * link = shift(&queue);

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert (link->message.type < MSG_TYPES);
#endif

	/* unlink from tqueue */
	struct MsgList * tqueue = &tqueues[link->message.type];
#ifdef BUF_ASSERT
	assert (tqueue->head == link);
#endif
	tqueue->head = link->qnext;
	if (!tqueue->head)
		tqueue->tail = NULL;
	link->qnext = NULL;
	--tqueue->size;

	return link;
}

/** Unqueue the first element of a list and return it.
 * \param list list container
 * \return first element of the list or NULL if list was empty
 */
static inline struct MsgLink * shiftTQueue(struct MsgList * list)
{
	if (!list->head)
		return NULL;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
	assert(!!queue.head == !!queue.tail);
#endif

	struct MsgLink * link = list->head;

#ifdef BUF_ASSERT
	assert (link->message.type == list - tqueues);
#endif

	/* unlink from queue */
	if (link->prev)
		link->prev->next = link->next;
	if (link->next)
		link->next->prev = link->prev;
	if (queue.head == link)
		queue.head = link->next;
	if (queue.tail == link)
		queue.tail = link->prev;
	link->next = link->prev = NULL;
	--queue.size;

	/* unlink from tqueue */
	list->head = link->qnext;
	if (!list->head)
		list->tail = NULL;
	link->qnext = NULL;
	--list->size;

	link->message.type = MSG_INVALID;

#if defined BUF_ASSERT && BUF_ASSERT >= 2
	assert(!!list->head == !!list->tail);
	assert(!!queue.head == !!queue.tail);
#endif

	return link;
}

/** Enqueue a new message at front of a list.
 * \param list list container
 * \param msg the message to be enqueued
 */
static inline void unshiftQueue(struct MsgLink * msg)
{
	assert (!msg->next && !msg->prev && !msg->qnext);

	unshift(&queue, msg);

	/* link into tqueue */
	struct MsgList * tqueue = &tqueues[msg->message.type];
	msg->qnext = tqueue->head;
	tqueue->head = msg;
	if (!tqueue->tail)
		tqueue->tail = msg;
	++tqueue->size;
}

/** Enqueue a new message at the end of a list.
 * \param list list container
 * \param msg message to be appended to the list
 */
static inline void pushQueue(struct MsgLink * msg)
{
#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
	assert (!msg->next && !msg->prev && !msg->qnext);
#endif

	push(&queue, msg);

	/* link into tqueue */
	struct MsgList * tqueue = &tqueues[msg->message.type];
	msg->qnext = NULL;
	if (!tqueue->head)
		tqueue->head = msg;
	else
		tqueue->tail->qnext = msg;
	tqueue->tail = msg;
	++tqueue->size;
}

static inline void clearQueue()
{
	clear(&queue);
	int i;
	for (i = 0; i < MSG_TYPES; ++i)
		clear(&tqueues[i]);
}

#if defined(BUF_ASSERT) && BUF_ASSERT >= 3
static void checkList(const struct MsgList * list)
{
	const struct MsgLink * msg, * prev = NULL;
	size_t size = 0;

	assert(!!list->head == !!list->tail);
	assert (list->size != 0 || list->head == NULL);
	assert (list->size > 1 || list->head == list->tail);

	for (msg = list->head; msg != NULL; msg = msg->next) {
		++size;
		assert (prev == msg->prev);
		prev = msg;
	}

	assert (size == list->size);
	assert (prev == list->tail);
}

static void checkTQueue(enum MSG_TYPE type)
{
	const struct MsgList * list = &tqueues[type];
	const struct MsgLink * msg, * prev = NULL;
	size_t size = 0;

	assert(!!list->head == !!list->tail);
	assert (list->size != 0 || list->head == NULL);
	assert (list->size > 1 || list->head == list->tail);

	for (msg = list->head; msg; msg = msg->qnext) {
		assert (msg->message.type == type);
		prev = msg;
		++size;
	}

	assert (size == list->size);
	assert (prev == list->tail);

	for (msg = queue.head; msg; msg = msg->next) {
		if (msg->message.type == type) {
			assert (list->head == msg);
			break;
		}
	}

	for (msg = queue.tail; msg; msg = msg->prev) {
		if (msg->message.type == type) {
			assert (list->tail == msg);
			break;
		}
	}
}

static void checkLists()
{
	checkList(&pool);
	checkList(&queue);

	struct MsgLink * msg;
	for (msg = queue.head; msg; msg = msg->next)
		assert (msg->message.type < MSG_TYPES);

	for (msg = pool.head; msg; msg = msg->next) {
		assert (msg->message.type == MSG_INVALID);
		assert (!msg->qnext);
	}

	int i;
	size_t sz = 0;
	for (i = 0; i < MSG_TYPES; ++i) {
		checkTQueue(i);
		sz += tqueues[i].size;
	}
	assert (sz == queue.size);
}
#endif
