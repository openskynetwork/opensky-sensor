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

/** Define to 1 to enable debugging messages */
#define BUF_DEBUG 1

/* Forward declaration */
struct Pool;

/** Linked Frame List */
struct FrameLink {
	/** Frame */
	struct ADSB_Frame frame;
	/** Next Element */
	struct FrameLink * next;
	/** Containing pool */
	struct Pool * pool;
};

/** Linked List Container */
struct FrameList {
	/** Head Pointer or NULL if empty */
	struct FrameLink * head;
	/** Tail Pointer or NULL if empty */
	struct FrameLink * tail;
	/** Size of list */
	size_t size;
};

/** Buffer Pool */
struct Pool {
	/** Pool start */
	struct FrameLink * pool;
	/** Pool size */
	size_t size;
	/** Linked List: next pool */
	struct Pool * next;
	/** Garbage collection: collected frames */
	struct FrameList collect;
};

/** Static Pool (those frames are always allocated in advance) */
static struct Pool staticPool;

/** Dynamic Pool max. increments */
static size_t dynMaxIncrements;
/** Dynamic Pool increments */
static size_t dynIncrements;
/** Dynamic Pool List */
static struct Pool * dynPools;

/** Number of frames discarded in an overflow situation */
static uint64_t overCapacity;
static uint64_t overCapacityMax;

/** Overall Pool */
static volatile struct FrameList pool;
/** Output Queue */
static volatile struct FrameList queue;

/** Currently filled frame (by writer), mainly for debugging purposes */
static struct FrameLink * newFrame;
/** Currently processed frame (by reader), for debugging purposes */
static struct FrameLink * currentFrame;

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

static inline struct FrameLink * shift(volatile struct FrameList * list);
static inline void unshift(volatile struct FrameList * list,
	struct FrameLink * frame);
static inline void push(volatile struct FrameList * list,
	struct FrameLink * frame);
static inline void append(volatile struct FrameList * dstList,
	const volatile struct FrameList * srcList);
static inline void clear(volatile struct FrameList * list);

/** Initialize frame buffer.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 */
static void construct()
{
	dynIncrements = 0;

	dynPools = NULL;
	clear(&pool);
	clear(&queue);

	newFrame = currentFrame = NULL;

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
			puts("BUF: Running Garbage Collector");
#endif
			collectPools();
			destroyUnusedPools();
		}
		pthread_mutex_unlock(&mutex);
	}
}

/** Flush Buffer queue. This discards all buffered but not processed frames */
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

/** Get a new frame from the pool. Extend the pool if there are more dynamic
 *  pools to get. Discard oldest frame if there is no more dynamic pool
 * \return a frame, which can be filled
 */
static struct FrameLink * getFrameFromPool()
{
	struct FrameLink * ret;

	if (pool.head) {
		/* pool is not empty -> unlink and return first frame */
		ret = shift(&pool);
		overCapacity = 0;
	} else if (uncollectPools()) {
		/* pool was empty, but we could uncollect a pool which was about to be
		 *  garbage collected */
#ifdef BUF_DEBUG
		puts("BUF: Uncollected pool");
#endif
		++STAT_stats.BUF_uncollects;
		overCapacity = 0;
		ret = shift(&pool);
	} else if (dynIncrements < dynMaxIncrements && createDynPool()) {
		/* pool was empty, but we just created another one */
#ifdef BUF_DEBUG
		printf("BUF: Created another pool (%zu/%zu)\n", dynIncrements,
			dynMaxIncrements);
#endif
		overCapacity = 0;
		ret = shift(&pool);
	} else {
		/* no more space in the pool and no more pools
		 * -> sacrifice oldest frame */
#if defined(BUF_DEBUG) && BUF_DEBUG > 1
		puts("BUF: Sacrificing oldest frame\n");
#endif
		if (++overCapacity > overCapacityMax)
			overCapacityMax = overCapacity;
		++STAT_stats.BUF_sacrifices;
		ret = shift(&queue);
	}

	assert (ret);
	return ret;
}

/** Get a frame from the unused frame pool for the writer in order to fill it.
 * \return new frame
 */
struct ADSB_Frame * BUF_newFrame()
{
	assert (!newFrame);
	pthread_mutex_lock(&mutex);
	newFrame = getFrameFromPool();
	pthread_mutex_unlock(&mutex);
	assert (newFrame);
	return &newFrame->frame;
}

/** Commit a filled frame from writer to reader queue.
 * \param frame filled frame to be delivered to the reader
 */
void BUF_commitFrame(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (&newFrame->frame == frame);

	pthread_mutex_lock(&mutex);
	push(&queue, newFrame);
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);

	if (queue.size > STAT_stats.BUF_maxQueue)
		STAT_stats.BUF_maxQueue = queue.size;

	newFrame = NULL;
}

/** Abort filling a frame and return it to the pool.
 * \param frame aborted frame
 */
void BUF_abortFrame(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (&newFrame->frame == frame);

	pthread_mutex_lock(&mutex);
	push(&pool, newFrame);
	newFrame = NULL;
	pthread_mutex_unlock(&mutex);

	newFrame = NULL;
}

/** Get a frame from the queue.
 * \return adsb frame
 */
const struct ADSB_Frame * BUF_getFrame()
{
	assert (!currentFrame);

	pthread_mutex_lock(&mutex);
	while (!queue.head) {
		int r = pthread_cond_wait(&cond, &mutex);
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	}
	currentFrame = shift(&queue);
	pthread_mutex_unlock(&mutex);

	return &currentFrame->frame;
}

/** Get a frame from the queue.
 * \param timeout_ms timeout in milliseconds
 * \return adsb frame or NULL on timeout
 */
const struct ADSB_Frame * BUF_getFrameTimeout(uint32_t timeout_ms)
{
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		++ts.tv_sec;
		ts.tv_nsec -= 1000000000;
	}

	assert (!currentFrame);

	pthread_mutex_lock(&mutex);
	while (!queue.head) {
		int r = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (r == ETIMEDOUT) {
			pthread_mutex_unlock(&mutex);
			return NULL;
		} else if (r) {
			error(-1, r, "pthread_cond_wait failed");
		}
	}
	currentFrame = shift(&queue);
	pthread_mutex_unlock(&mutex);

	return &currentFrame->frame;
}

/** Put a frame back into the pool.
 * \note Can be called by the reader after a frame is processed in order to
 *  put the frame into the pool again. If it is not called explicitly, it is
 *  called by the next call of FB_get implicitly.
 * \param frame frame to be put, must be the last one returned by one of
 *  BUF_getFrame() or BUF_getFrameTimeout()
 */
void BUF_releaseFrame(const struct ADSB_Frame * frame)
{
	assert (frame);
	assert (frame == &currentFrame->frame);

	pthread_mutex_lock(&mutex);
	push(&pool, currentFrame);
	pthread_mutex_unlock(&mutex);
	currentFrame = NULL;
}

/** Put a frame back into the queue.
 * \note This is meant for unsuccessful behavior of the reader. It should be
 *  called if the reader wants to get that frame again next time when
 *  BUF_getFrame() or BUF_getFrameTimeout() is called.
 * \param frame frame to be put, must be the last one returned by one of
 *  BUF_getFrame() or BUF_getFrameTimeout()
 */
void BUF_putFrame(const struct ADSB_Frame * frame)
{
	assert (frame);
	assert (frame == &currentFrame->frame);

	pthread_mutex_lock(&mutex);
	unshift(&queue, currentFrame);
	pthread_mutex_unlock(&mutex);
	currentFrame = NULL;
}

/** Deploy a new pool: allocate frames and append them to the overall pool.
 * \param newPool pool to be deployed
 * \param size number of frames in that pool
 * \return true if deployment succeeded, false if allocation failed
 */
static bool deployPool(struct Pool * newPool, size_t size)
{
	/* allocate new frames */
	struct FrameLink * initFrames = malloc(size * sizeof *initFrames);
	if (!initFrames)
		return false;

	/* initialize pool */
	newPool->pool = initFrames;
	newPool->size = size;
	newPool->next = NULL;
	clear(&newPool->collect);

	/* setup frame list */
	struct FrameList list;
	list.head = initFrames;
	list.tail = initFrames + size - 1;
	size_t i;
	struct FrameLink * frame = initFrames;
	for (i = 0; i < size; ++i, ++frame) {
		frame->next = frame + 1;
		frame->pool = newPool;
	}
	list.tail->next = NULL;
	list.size = size;

	/* append frames to the overall pool */
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

/** Collect unused frames from the overall pool to their own pool.
 * \note used for garbage collection
 */
static void collectPools()
{
	struct FrameLink * frame, * prev = NULL, * next;
#ifdef BUF_DEBUG
	const size_t prevSize = pool.size;
#endif

	/* iterate over all frames of the pool and keep a pointer to the
	 * predecessor */
	for (frame = pool.head; frame; frame = next) {
		next = frame->next;
		if (frame->pool != &staticPool) {
			/* collect only frames of dynamic pools */

			/* unlink from the overall pool */
			if (prev)
				prev->next = next;
			else
				pool.head = next;
			if (frame == pool.tail)
				pool.tail = prev;
			--pool.size;

			/* add them to their pools' collection */
			unshift(&frame->pool->collect, frame);
		} else {
			prev = frame;
		}
	}
#ifdef BUF_DEBUG
	printf("BUF: Collected %zu of %zu frames from overall pool to their "
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
			/* pools' collection was not empty, put frames back into the
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

			/* delete frames */
			free(pool->pool);
			/* delete pool itself */
			free(pool);
		} else {
			prev = pool;
		}
	}
#ifdef BUF_DEBUG
	printf("BUF: Destroyed %zu of %zu dynamic pools\n",
		prevSize - dynIncrements, prevSize);
#endif
}

/** Unqueue the first element of a list and return it.
 * \param list list container
 * \return first element of the list or NULL if list was empty
 */
static inline struct FrameLink * shift(volatile struct FrameList * list)
{
	assert(!!list->head == !!list->tail);

	if (!list->head) /* empty list */
		return NULL;

	/* first element */
	struct FrameLink * ret = list->head;

	/* unlink */
	list->head = ret->next;
	if (list->tail == ret)
		list->tail = NULL;
	ret->next = NULL;
	--list->size;

	assert(!!list->head == !!list->tail);
	return ret;
}

/** Enqueue a new frame at front of a list.
 * \param list list container
 * \param frame the frame to be enqueued
 */
static inline void unshift(volatile struct FrameList * list,
	struct FrameLink * frame)
{
	assert(!!list->head == !!list->tail);

	/* frame will be the new head */
	frame->next = list->head;

	/* link into container */
	list->head = frame;
	if (!list->tail)
		list->tail = frame;
	++list->size;

	assert(!!list->head == !!list->tail);
}

/** Enqueue a new frame at the end of a list.
 * \param list list container
 * \param frame frame to be appended to the list
 */
static inline void push(volatile struct FrameList * list,
	struct FrameLink * frame)
{
	assert(!!list->head == !!list->tail);

	/* frame will be new tail */
	frame->next = NULL;

	/* link into container */
	if (!list->head) {
		list->head = list->tail = frame;
	} else {
		list->tail->next = frame;
		list->tail = frame;
	}
	++list->size;

	assert(!!list->head == !!list->tail);
}

/** Append a new frame list at the end of a list.
 * \param dstList destination list container
 * \param srcList source list container to be appended to the first list
 */
static inline void append(volatile struct FrameList * dstList,
	const volatile struct FrameList * srcList)
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
	}
	dstList->size += srcList->size;

	assert(!!dstList->head == !!dstList->tail);
}

static inline void clear(volatile struct FrameList * list)
{
	list->head = list->tail = NULL;
	list->size = 0;
}
