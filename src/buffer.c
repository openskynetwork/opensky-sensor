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
#include <unistd.h>
#include <statistics.h>
#include <cfgfile.h>
#include <threads.h>

/** Define to 1 to enable debugging messages */
#define BUF_DEBUG 1

/* Forward declaration */
struct Pool;

/** Linked Message List */
struct MsgLink {
	/** Message */
	struct ADSB_Frame message;
	/** Next Element */
	struct MsgLink * next;
	/** Previous Element */
	struct MsgLink * prev;
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

/** Number of messages discarded in an overflow situation */
static uint64_t overCapacity;
static uint64_t overCapacityMax;

/** Overall Pool */
static struct MsgList pool;
/** Output Queue */
static struct MsgList queue;

/** Currently produced frame, for debugging purposes */
static struct MsgLink * newFrame;
/** Currently consumed frame, for debugging purposes */
static struct MsgLink * currentMessage;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for listOut) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void construct();
static void destruct();
static void mainloop();
static bool start(struct Component * c, void * data);
static void stop(struct Component * c);

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
static inline void unshift(struct MsgList * list,
	struct MsgLink * msg);
static inline void push(struct MsgList * list,
	struct MsgLink * msg);
static inline void append(struct MsgList * dstList,
	const struct MsgList * srcList);
static inline void clear(struct MsgList * list);

/** Initialize message buffer. */
static void construct()
{
	dynIncrements = 0;

	dynPools = NULL;
	clear(&pool);
	clear(&queue);

	newFrame = currentMessage = NULL;

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
		if (queue.size <
			(dynIncrements * CFG_config.buf.dynBacklog) /
				CFG_config.buf.gcLevel) {
			++STAT_stats.BUF_GCRuns;
#ifdef BUF_DEBUG
			NOC_puts("BUF: Running Garbage Collector");
#endif
			collectPools();
			destroyUnusedPools();
		}
		pthread_mutex_unlock(&mutex);
	}
}

/** Flush Buffer queue. This discards all buffered but not processed messages */
void BUF_flush()
{
	pthread_mutex_lock(&mutex);
	append(&pool, &queue);
	clear(&queue);
	pthread_mutex_unlock(&mutex);
	++STAT_stats.BUF_flushes;
}

/** Fill missing statistics */
void BUF_fillStatistics()
{
	STAT_stats.BUF_pools = dynIncrements;
	STAT_stats.BUF_queue = queue.size;
	STAT_stats.BUF_pool = pool.size;
	STAT_stats.BUF_sacrificeMax = overCapacityMax;
}

/** Get a new message from the pool. Extend the pool if there are more dynamic
 *  pools to get. Discard oldest message if there is no more dynamic pool
 * \return a message which can be filled
 */
static struct MsgLink * getMessageFromPool()
{
	struct MsgLink * ret;

	if (pool.head) {
		/* pool is not empty -> unlink and return first message */
		ret = shift(&pool);
		overCapacity = 0;
	} else if (uncollectPools()) {
		/* pool was empty, but we could uncollect a pool which was about to be
		 *  garbage collected */
#ifdef BUF_DEBUG
		NOC_puts("BUF: Uncollected pool");
#endif
		++STAT_stats.BUF_uncollects;
		overCapacity = 0;
		ret = shift(&pool);
	} else if (dynIncrements < dynMaxIncrements && createDynPool()) {
		/* pool was empty, but we just created another one */
#ifdef BUF_DEBUG
		NOC_printf("BUF: Created another pool (%zu/%zu)\n", dynIncrements,
			dynMaxIncrements);
#endif
		overCapacity = 0;
		ret = shift(&pool);
	} else {
		/* no more space in the pool and no more pools
		 * -> sacrifice oldest message */
#if defined(BUF_DEBUG) && BUF_DEBUG > 1
		NOC_puts("BUF: Sacrificing oldest message");
#endif
		if (++overCapacity > overCapacityMax)
			overCapacityMax = overCapacity;
		++STAT_stats.BUF_sacrifices;
		ret = shift(&queue);
	}

	assert (ret);
	return ret;
}

/** Get a message from the unused message pool for the writer in order to fill
 *  it.
 * \return new message
 */
struct ADSB_Frame * BUF_newMessage()
{
	assert (!newFrame);
	pthread_mutex_lock(&mutex);
	newFrame = getMessageFromPool();
	pthread_mutex_unlock(&mutex);
	assert (newFrame);
	return &newFrame->message;
}

/** Commit a filled message from writer to reader queue.
 * \param msg filled message to be delivered to the reader
 */
void BUF_commitMessage(struct ADSB_Frame * msg)
{
	assert (msg);
	assert (&newFrame->message == msg);

	pthread_mutex_lock(&mutex);
	push(&queue, newFrame);
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);

	if (queue.size > STAT_stats.BUF_maxQueue)
		STAT_stats.BUF_maxQueue = queue.size;

	newFrame = NULL;
}

/** Abort filling a message and return it to the pool.
 * \param msg aborted message
 */
void BUF_abortMessage(struct ADSB_Frame * msg)
{
	assert (msg);
	assert (&newFrame->message == msg);

	pthread_mutex_lock(&mutex);
	unshift(&pool, newFrame);
	pthread_mutex_unlock(&mutex);

	newFrame = NULL;
}

static void cleanup(void * dummy)
{
	pthread_mutex_unlock(&mutex);
}

/** Get a message from the queue.
 * \return message
 */
const struct ADSB_Frame * BUF_getMessage()
{
	assert (!currentMessage);
	CLEANUP_PUSH(&cleanup, NULL);
	pthread_mutex_lock(&mutex);
	while (!queue.head) {
		int r = pthread_cond_wait(&cond, &mutex);
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	}
	currentMessage = shift(&queue);
	CLEANUP_POP();
	return &currentMessage->message;
}

/** Get a message from the queue.
 * \param timeout_ms timeout in milliseconds
 * \return message or NULL on timeout
 */
const struct ADSB_Frame * BUF_getMessageTimeout(uint32_t timeout_ms)
{
	struct timespec ts;
	struct ADSB_Frame * ret;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		++ts.tv_sec;
		ts.tv_nsec -= 1000000000;
	}

	assert (!currentMessage);

	pthread_mutex_lock(&mutex);
	CLEANUP_PUSH(&cleanup, NULL);
	while (!queue.head) {
		int r = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (r == ETIMEDOUT) {
			ret = NULL;
			break;
		} else if (r) {
			error(-1, r, "pthread_cond_timedwait failed");
		}
	}
	if (queue.head) {
		currentMessage = shift(&queue);
		ret = &currentMessage->message;
	}

	CLEANUP_POP();

	return ret;
}

/** Put a message back into the pool.
 * \note Can be called by the reader after a message is processed in order to
 *  put the message into the pool again.
 * \param msg message to be put, must be the last one returned by one of
 *  BUF_getMessage() or BUF_getMessageTimeout()
 */
void BUF_releaseMessage(const struct ADSB_Frame * msg)
{
	assert (msg);
	assert (msg == &currentMessage->message);

	pthread_mutex_lock(&mutex);
	unshift(&pool, currentMessage);
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
void BUF_putMessage(const struct ADSB_Frame * msg)
{
	assert (msg);
	assert (msg == &currentMessage->message);

	pthread_mutex_lock(&mutex);
	unshift(&queue, currentMessage);
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
	append(&pool, &list);

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
			append(&pool, &p->collect);
			/* clear collection */
			p->collect.head = p->collect.tail = NULL;
			p->collect.size = 0;
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
	assert(!!list->head == !!list->tail);

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

	assert(!!list->head == !!list->tail);
	return ret;
}

/** Enqueue a new message at front of a list.
 * \param list list container
 * \param msg the message to be enqueued
 */
static inline void unshift(struct MsgList * list, struct MsgLink * msg)
{
	assert(!!list->head == !!list->tail);

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

	assert(!!list->head == !!list->tail);
}

/** Enqueue a new message at the end of a list.
 * \param list list container
 * \param msg message to be appended to the list
 */
static inline void push(struct MsgList * list, struct MsgLink * msg)
{
	assert(!!list->head == !!list->tail);

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

	assert(!!list->head == !!list->tail);
}

/** Append a new message list at the end of a list.
 * \param dstList destination list container
 * \param srcList source list container to be appended to the first list
 */
static inline void append(struct MsgList * dstList,
	const struct MsgList * srcList)
{
	assert(!!dstList->head == !!dstList->tail);
	assert(!!srcList->head == !!srcList->tail);

	if (!dstList->head) /* link head if dst is empty */
		dstList->head = srcList->head;
	else /* link last element otherwise */
		dstList->tail->next = srcList->head;
	if (srcList->tail) {
		/* link tail */
		dstList->tail = srcList->tail;
		srcList->head->prev = dstList->tail;
	}
	dstList->size += srcList->size;

	assert(!!dstList->head == !!dstList->tail);
}

static inline void clear(struct MsgList * list)
{
	list->head = list->tail = NULL;
	list->size = 0;
}
