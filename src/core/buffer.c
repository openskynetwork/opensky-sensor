/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "buffer.h"
#include "util/log.h"
#include "util/cfgfile.h"
#include "util/threads.h"
#include "util/util.h"
#include "util/list.h"
#include "util/port/misc.h"

/** Component: Prefix */
static const char PFX[] = "BUF";

/* Define to 1 to enable debugging messages */
#define BUF_DEBUG 1

/* Forward declaration */
struct Pool;

/** Container for an OpenSky frame */
struct FrameLink {
	/** Frame */
	struct OPENSKY_RawFrame frame;
	/** Linked list: siblings */
	struct LIST_LinkD list;
	/** Containing pool */
	struct Pool * pool;
};

/** Buffer Pool */
struct Pool {
	/** Pool: vector of frame links */
	struct FrameLink * links;
	/** Number of frame links */
	size_t size;
	/** Linked List: siblings */
	struct LIST_LinkD list;
	/** Garbage collection: collected frames */
	struct LIST_ListDC collect;
};

/** Static Pool (those frames are always allocated in advance) */
static struct Pool staticPool;

/** Dynamic Pool: max number of dynamic pools */
static size_t dynMaxPools;
/** Dynamic Pool: list of dynamic pools */
static struct LIST_ListDC dynPools;

/** Overall frame pool */
static struct LIST_ListDC pool;
/** Output frame queue */
static struct LIST_ListDC queue;

/** Currently produced frame, mainly for debugging purposes */
static struct FrameLink * newFrame;
/** Currently consumed frame, mainly for debugging purposes */
static struct FrameLink * currentFrame;

/** Queue and Pool Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for queue) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

/** Configuration */
struct Configuration {
	/** number of frames in static pool */
	uint32_t statBacklog;
	/** number of frames in each dynamic pool */
	uint32_t dynBacklog;
	/** number of dynamic pools */
	uint32_t dynIncrement;
	/** enable history (if not, no dynamic pools are created and the buffer is
	 * flushed upon reconnect */
	bool history;
	/** enable garbage collector */
	bool gc;
	/** garbage collector interval */
	uint32_t gcInterval;
	/** garbage collector threshold */
	uint32_t gcLevel;
};

/** Configuration */
static struct Configuration cfg;

/** Statistics */
static struct BUFFER_Statistics stats;

static bool construct();
static void destruct();
static void mainloop();
static void resetStats();
static void cfgFix(const struct CFG_Section * sect);

static bool deployPool(struct Pool * newPool, size_t size);
static bool createDynPool();
static void collectPools();
static bool uncollectPools();
static void destroyUnusedPools();
static void gc();

/** Configuration: Descriptor */
static const struct CFG_Section cfgDesc = {
	.name = "BUFFER",
	.n_opt = 7,
	.fix = &cfgFix,
	.options = {
		{
			.name = "StaticBacklog",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfg.statBacklog,
			.def = { .integer = 200 }
		},
		{
			.name = "DynamicBacklog",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfg.dynBacklog,
			.def = { .integer = 1000 }
		},
		{
			.name = "DynamicIncrements",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfg.dynIncrement,
			.def = { .integer = 1080 }
		},
		{
			.name = "History",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfg.history,
			.def = { .boolean = false }
		},
		{
			.name = "GC",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfg.gc,
			.def = { .boolean = false }
		},
		{
			.name = "GCInterval",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfg.gcInterval,
			.def = { .integer = 120 }
		},
		{
			.name = "GCLevel",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfg.gcLevel,
			.def = { .integer = 2 }
		},
	}
};

const struct Component BUF_comp = {
	.description = PFX,
	.start = &cfg.gc,
	.onConstruct = &construct,
	.onDestruct = &destruct,
	.main = &mainloop,
	.config = &cfgDesc,
	.onReset = &resetStats,
	.dependencies = { NULL }
};

/** Configuration: Fix configuration.
 * @param sect configuration section, should be @cfgDesc
 */
static void cfgFix(const struct CFG_Section * sect)
{
	assert (sect == &cfgDesc);
	if (cfg.statBacklog < 2) {
		cfg.statBacklog = 2;
		LOG_log(LOG_LEVEL_WARN, PFX, "BUFFER.staticBacklog was increased to 2");
	}

	if (cfg.gc && !cfg.history) {
		cfg.gc = false;
		LOG_log(LOG_LEVEL_WARN, PFX, "Ignoring BUFFER.GC because "
			"BUFFER.history is not enabled");
	}
}

/** Component: Initialize frame buffer.
 * @return true if buffer could be initialized, false otherwise
 */
static bool construct()
{
	assert(cfg.statBacklog >= 2);

	/* initialize all data structures */
	LIST_init(&dynPools);
	LIST_init(&pool);
	LIST_init(&queue);

	newFrame = currentFrame = NULL;

	dynMaxPools = cfg.history ? cfg.dynIncrement : 0;

	/* deploy static pool */
	if (!deployPool(&staticPool, cfg.statBacklog)) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "malloc failed: could not allocate "
			"static pool");
		return false;
	} else
		return true;
}

/** Component: destruct frame buffer. */
static void destruct()
{
	/* delete all dynamic pools */
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&dynPools, l, n) {
		struct Pool * dyn = LIST_item(l, struct Pool, list);
		free(dyn->links);
		free(dyn);
	}
	LIST_init(&dynPools);

	/** delete static pool */
	free(staticPool.links);
}

/** Gargabe collector mainloop.
 * \note initialize garbage collection first using BUF_initGC */
static void mainloop()
{
	while (true) {
		sleepCancelable(cfg.gcInterval);
		pthread_mutex_lock(&mutex);
		if (LIST_length(&queue) <
			(LIST_length(&dynPools) * cfg.dynBacklog) / cfg.gcLevel) {
			/* threshold met -> run garbage collector */
			gc();
		}
		pthread_mutex_unlock(&mutex);
	}
}

/** Manually trigger garbage collection */
void BUF_runGC()
{
	pthread_mutex_lock(&mutex);
	gc();
	pthread_mutex_unlock(&mutex);
}

/** Run garbage collector: collect all unused frames from the overall pool
 * to the dynamic pool's collected list. If all frames of the pool have been
 * collected, delete the pool.
 * There's a good chance, that at least one frame of at least one dynamic pool
 * is in use. The chance will decrease dramically if the garbage collector is
 * running twice after a jiffy.
 */
static void gc()
{
	++stats.GCruns;
#ifdef BUF_DEBUG
	LOG_log(LOG_LEVEL_DEBUG, PFX, "Running Garbage Collector");
#endif
	collectPools();
	destroyUnusedPools();
}

/** Flush buffer queue. This discards all buffered but not processed frames */
void BUF_flush()
{
	pthread_mutex_lock(&mutex);
	LIST_concatenatePush(&pool, &queue);
	++stats.flushes;
	pthread_mutex_unlock(&mutex);
}

/** Flush buffer queue if history is disabled. This discards all buffered but
 * not processed frames */
void BUF_flushUnlessHistoryEnabled()
{
	if (!cfg.history)
		BUF_flush();
}

/** Get a new frame from the pool. Extend the pool if there are more dynamic
 *  pools to get. Discard oldest frame if there is no more dynamic pool.
 * @note always succeeds, never waits
 * @return a frame which is about to be produced
 */
static inline struct FrameLink * getFrameFromPool()
{
	struct FrameLink * ret;

	if (likely(!LIST_empty(&pool))) {
		/* pool is not empty -> unlink and return first frame */
		ret = LIST_itemSafe(&pool, LIST_shift(&pool), struct FrameLink, list);
		stats.discardedCur = 0;
	} else if (uncollectPools()) {
		/* pool was empty, but we could uncollect a pool which was about to be
		 *  garbage collected */
#ifdef BUF_DEBUG
		LOG_log(LOG_LEVEL_DEBUG, PFX, "Uncollected pool");
#endif
		++stats.uncollectedPools;
		stats.discardedCur = 0;
		ret = LIST_itemSafe(&pool, LIST_shift(&pool), struct FrameLink, list);
	} else if (LIST_length(&dynPools) < dynMaxPools && createDynPool()) {
		/* pool was empty, but we just created another one */
#ifdef BUF_DEBUG
		LOG_logf(LOG_LEVEL_DEBUG, PFX, "Created another pool (%" PRI_SIZE_T
			"/%" PRI_SIZE_T ")", LIST_length(&dynPools), dynMaxPools);
#endif
		stats.discardedCur = 0;
		ret = LIST_itemSafe(&pool, LIST_shift(&pool), struct FrameLink, list);
	} else {
		/* no more frames in the pool and no more pools
		 *  -> sacrifice oldest frame */
#if defined(BUF_DEBUG) && BUF_DEBUG > 1
		LOG_log(LOG_LEVEL_DEBUG, PFX, "Sacrificing oldest frame");
#endif
		++stats.discardedCur;
		++stats.discardedAll;
		if (stats.discardedCur > stats.discardedMax)
			stats.discardedMax = stats.discardedCur;
		ret = LIST_itemSafe(&queue, LIST_shift(&queue), struct FrameLink, list);
	}

	assert(ret);
	return ret;
}

/** Producer: get a frame from the unused frame pool for the producer in order
 * to fill it.
 * If the pool is empty, new pools are allocated up to a configured maximum.
 * If the maximum is reached, the oldest frame will be discarded.
 * @note always succeeds, never waits
 * @return new frame to be filled
 */
struct OPENSKY_RawFrame * BUF_newFrame()
{
	assert(!newFrame);
	pthread_mutex_lock(&mutex);
	newFrame = getFrameFromPool();
	assert(newFrame != currentFrame);
	pthread_mutex_unlock(&mutex);
	assert(newFrame);
	return &newFrame->frame;
}

/** Producer: commit a produced frame to the consumer queue.
 * @param frame filled frame to be delivered to the consumer
 */
void BUF_commitFrame(struct OPENSKY_RawFrame * frame)
{
	assert(frame);
	assert(&newFrame->frame == frame);

	pthread_mutex_lock(&mutex);
	LIST_push(&queue, LIST_link(newFrame, list));
	pthread_cond_broadcast(&cond);

	if (unlikely(LIST_length(&queue) > stats.maxQueueSize))
		stats.maxQueueSize = LIST_length(&queue);
	pthread_mutex_unlock(&mutex);

	newFrame = NULL;
}

/** Producer: abort producing a frame and return it to the pool.
 * @param frame aborted frame
 */
void BUF_abortFrame(struct OPENSKY_RawFrame * frame)
{
	assert(frame);
	assert(&newFrame->frame == frame);

	pthread_mutex_lock(&mutex);
	LIST_unshift(&pool, LIST_link(newFrame, list));
	pthread_mutex_unlock(&mutex);

	newFrame = NULL;
}

/** Consumer: get a frame from the queue. Waits (infinitely) if the queue is
 * empty.
 * @return frame
 */
const struct OPENSKY_RawFrame * BUF_getFrame()
{
	assert(!currentFrame);
	CLEANUP_PUSH_LOCK(&mutex);
	while (LIST_empty(&queue)) {
		int r = pthread_cond_wait(&cond, &mutex);
		if (r)
			LOG_errno2(LOG_LEVEL_EMERG, r, PFX, "pthread_cond_wait failed");
	}
	currentFrame = LIST_item(LIST_shift(&queue), struct FrameLink, list);
	CLEANUP_POP();
	return &currentFrame->frame;
}

/** Consumer: get a frame from the queue with timeout.
 * @param timeout_ms timeout in milliseconds
 * @return frame or NULL on timeout
 */
const struct OPENSKY_RawFrame * BUF_getFrameTimeout(uint_fast32_t timeout_ms)
{
	struct timespec ts;
	struct OPENSKY_RawFrame * ret;

	/* get an absolute point in time from the relative timeout */
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		++ts.tv_sec;
		ts.tv_nsec -= 1000000000;
	}

	assert(!currentFrame);

	CLEANUP_PUSH_LOCK(&mutex);
	while (LIST_empty(&queue)) {
		int r = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (r == ETIMEDOUT) {
			/* timeout */
			ret = NULL;
			goto cleanup;
		} else if (r) {
			LOG_errno2(LOG_LEVEL_EMERG, r, PFX,
				"pthread_cond_timedwait failed");
		}
	}
	currentFrame = LIST_item(LIST_shift(&queue), struct FrameLink, list);
	ret = &currentFrame->frame;
cleanup:
	CLEANUP_POP();

	return ret;
}

/** Consumer: put a frame back into the pool.
 * @note Called after a frame is consumed in order to put the frame into the
 *  pool again.
 * @param frame frame to be put, must be the last one returned by one of
 *  BUF_getFrame() or BUF_getFrameTimeout()
 */
void BUF_releaseFrame(const struct OPENSKY_RawFrame * frame)
{
	assert(frame);
	assert(frame == &currentFrame->frame);

	pthread_mutex_lock(&mutex);
	LIST_unshift(&pool, LIST_link(currentFrame, list));
	currentFrame = NULL;
	pthread_mutex_unlock(&mutex);
}

/** Consumer: Put a frame back into the queue.
 * @note This is meant for unsuccessful behavior of the consumer. It should be
 *  called if the consumer wants to get that frame again next time when
 *  BUF_getFrame() or BUF_getFrameTimeout() is called.
 * @param frame frame to be put, must be the last one returned by one of
 *  BUF_getFrame() or BUF_getFrameTimeout()
 */
void BUF_putFrame(const struct OPENSKY_RawFrame * frame)
{
	assert(frame);
	assert(frame == &currentFrame->frame);

	pthread_mutex_lock(&mutex);
	LIST_unshift(&queue, LIST_link(currentFrame, list));
	currentFrame = NULL;
	pthread_mutex_unlock(&mutex);
}

/** Deploy a new pool: allocate frames and append them to the overall pool.
 * @param newPool pool to be deployed
 * @param size number of frames in that pool
 * @return true if deployment succeeded, false if allocation failed
 */
static bool deployPool(struct Pool * newPool, size_t size)
{
	/* allocate new frames */
	struct FrameLink * initFrames = malloc(size * sizeof *initFrames);
	if (unlikely(initFrames == NULL))
		return false;

	/* initialize pool */
	newPool->links = initFrames;
	newPool->size = size;
	LIST_init(&newPool->collect);

	/* establish back links */
	size_t i;
	for (i = 0; i < size; ++i)
		initFrames[i].pool = newPool;

	/* append frames to the overall pool */
	struct LIST_ListDC list = LIST_ListDC_INIT(list);
	LIST_fromVector(&list, initFrames, list, size);
	LIST_concatenatePush(&pool, &list);

	return true;
}

/** Create a new pool, deploy it and add it to the list of pools.
 * @return true if operation succeeded, false if allocation failed
 */
static bool createDynPool()
{
	/* allocate pool */
	struct Pool * newPool = malloc(sizeof *newPool);
	if (!newPool)
		return false;

	/* deployment */
	if (unlikely(!deployPool(newPool, cfg.dynBacklog))) {
		free(newPool);
		return false;
	}

	/* add pool to the list of dynamic pools */
	LIST_push(&dynPools, LIST_link(newPool, list));

	++stats.dynPoolsAll;
	if (unlikely(LIST_length(&dynPools) > stats.dynPoolsMax))
		stats.dynPoolsMax = LIST_length(&dynPools);

	return true;
}

/** Garbage collector: collect unused frames from the overall pool to their own
 *  pool.
 */
static void collectPools()
{
#ifdef BUF_DEBUG
	const size_t prevSize = LIST_length(&pool);
#endif

	/* iterate over all frames of the pool */
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&pool, l, n) {
		struct FrameLink * frame = LIST_item(l, struct FrameLink, list);

		if (frame->pool != &staticPool) {
			/* collect only frames of dynamic pools */

			LIST_unlink(&pool, l);
			LIST_unshift(&frame->pool->collect, l);
		}
	}
#ifdef BUF_DEBUG
	LOG_logf(LOG_LEVEL_DEBUG, PFX, "Collected %" PRI_SIZE_T " of %" PRI_SIZE_T
		" frames from overall pool to their dynamic pools",
		prevSize - LIST_length(&pool), prevSize);
#endif
}

/** Garbage collector: revert first stage of the garbage collection.
 * @return true if a dynamic pool was uncollected
 *  (i.e. there are new elements in the overall pool)
 */
static bool uncollectPools()
{
	struct Pool * p;

	/* iterate over all dynamic pools */
	LIST_foreachItem(&dynPools, p, list) {
		if (!LIST_empty(&p->collect)) {
			/* pools' collection was not empty, put frames back into the
			 * overall pool */
			LIST_concatenatePush(&pool, &p->collect);
			return true;
		}
	}
	/* no such pool found */
	return false;
}

/** Garbage collector: free all fully collected pools. */
static void destroyUnusedPools()
{
#ifdef BUF_DEBUG
	const size_t prevSize = LIST_length(&dynPools);
#endif

	/* iterate over all dynamic pools */
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&dynPools, l, n) {
		struct Pool * pool = LIST_item(l, struct Pool, list);
		if (likely(LIST_length(&pool->collect) ==  pool->size)) {
			/* fully collected pool */

			/* remove pool from the list of pools */
			LIST_unlink(&dynPools, l);

			/* delete frames */
			free(pool->links);
			/* delete pool itself */
			free(pool);
		}
	}
#ifdef BUF_DEBUG
	LOG_logf(LOG_LEVEL_DEBUG, PFX, "Destroyed %" PRI_SIZE_T " of %" PRI_SIZE_T
		" dynamic pools", prevSize - LIST_length(&dynPools), prevSize);
#endif
}

static void resetStats()
{
	pthread_mutex_lock(&mutex);
	memset(&stats, 0, sizeof stats);
	pthread_mutex_unlock(&mutex);
}

void BUFFER_getStatistics(struct BUFFER_Statistics * statistics)
{
	pthread_mutex_lock(&mutex);
	memcpy(statistics, &stats, sizeof *statistics);
	statistics->queueSize = LIST_length(&queue);
	statistics->poolSize = LIST_length(&pool);
	statistics->dynPools = LIST_length(&dynPools);
	pthread_mutex_unlock(&mutex);
}

