/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

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

static const char PFX[] = "BUF";

/** Define to 1 to enable debugging messages */
//#define BUF_DEBUG 1

/* Forward declaration */
struct Pool;

/** Linked Frame List */
struct FrameLink {
	/** Frame */
	struct OPENSKY_RawFrame frame;
	struct LIST_LinkD list;
	/** Containing pool */
	struct Pool * pool;
};

/** Buffer Pool */
struct Pool {
	struct FrameLink * links;
	size_t size;
	/** Linked List: next pool */
	struct LIST_LinkD list;
	/** Garbage collection: collected frames */
	struct LIST_ListDC collect;
};

/** Static Pool (those frames are always allocated in advance) */
static struct Pool staticPool;

/** Dynamic Pool max. increments */
static size_t dynMaxIncrements;
/** Dynamic Pool List */
static struct LIST_ListDC dynPools;

/** Overall Pool */
static struct LIST_ListDC pool;
/** Output Queue */
static struct LIST_ListDC queue;

/** Currently produced frame, for debugging purposes */
static struct FrameLink * newFrame;
/** Currently consumed frame, for debugging purposes */
static struct FrameLink * currentFrame;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for listOut) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static uint32_t cfgStatBacklog;
static uint32_t cfgDynBacklog;
static uint32_t cfgDynIncrement;
static bool cfgHistory;
static bool cfgGC;
static uint32_t cfgGCInterval;
static uint32_t cfgGCLevel;

static bool construct();
static void destruct();
static void mainloop();
static void resetStats();
static void cfgFix(const struct CFG_Section * sect);

static const struct CFG_Section cfg = {
	.name = "BUFFER",
	.n_opt = 7,
	.fix = &cfgFix,
	.options = {
		{
			.name = "StaticBacklog",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgStatBacklog,
			.def = {
				.integer = 200
			}
		},
		{
			.name = "DynamicBacklog",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgDynBacklog,
			.def = {
				.integer = 1000
			}
		},
		{
			.name = "DynamicIncrements",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgDynIncrement,
			.def = {
				.integer = 1080
			}
		},
		{
			.name = "History",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfgHistory,
			.def = { .boolean = false }
		},
		{
			.name = "GC",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfgGC,
			.def = { .boolean = false }
		},
		{
			.name = "GCInterval",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgGCInterval,
			.def = { .integer = 120 }
		},
		{
			.name = "GCLevel",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgGCLevel,
			.def = { .integer = 2 }
		},
	}
};

static struct BUFFER_Statistics stats;

const struct Component BUF_comp = {
	.description = PFX,
	.start = &cfgGC,
	.onConstruct = &construct,
	.onDestruct = &destruct,
	.main = &mainloop,
	.config = &cfg,
	.onReset = &resetStats,
	.dependencies = { NULL }
};

static void cfgFix(const struct CFG_Section * sect)
{
	assert (sect == &cfg);
	if (cfgStatBacklog < 2) {
		cfgStatBacklog = 2;
		LOG_log(LOG_LEVEL_WARN, PFX, "BUFFER.staticBacklog was increased to 2");
	}

	if (cfgGC && !cfgHistory) {
		cfgGC = false;
		LOG_log(LOG_LEVEL_WARN, PFX, "Ignoring BUFFER.GC because "
			"BUFFER.history is not enabled");
	}
}

static bool deployPool(struct Pool * newPool, size_t size);
static bool createDynPool();
static void collectPools();
static bool uncollectPools();
static void destroyUnusedPools();
static void gc();

/** Initialize frame buffer. */
static bool construct()
{
	assert(cfgStatBacklog >= 2);

	LIST_init(&dynPools);
	LIST_init(&pool);
	LIST_init(&queue);

	newFrame = currentFrame = NULL;

	dynMaxIncrements = cfgHistory ? cfgDynIncrement : 0;

	if (!deployPool(&staticPool, cfgStatBacklog)) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "malloc failed");
		return false;
	} else
		return true;
}

static void destruct()
{
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&dynPools, l, n) {
		struct Pool * dyn = LIST_item(l, struct Pool, list);
		free(dyn->links);
		free(dyn);
	}
	LIST_init(&dynPools);

	free(staticPool.links);
}

/** Gargabe collector mainloop.
 * \note initialize garbage collection first using BUF_initGC */
static void mainloop()
{
	while (true) {
		sleep(cfgGCInterval);
		pthread_mutex_lock(&mutex);
		if (LIST_length(&queue) <
			(LIST_length(&dynPools) * cfgDynBacklog) / cfgGCLevel) {
			gc();
		}
		pthread_mutex_unlock(&mutex);
	}
}

void BUF_runGC()
{
	pthread_mutex_lock(&mutex);
	gc();
	pthread_mutex_unlock(&mutex);
}

static void gc()
{
	++stats.GCruns;
#ifdef BUF_DEBUG
	LOG_log(LOG_LEVEL_DEBUG, PFX, "Running Garbage Collector");
#endif
	collectPools();
	destroyUnusedPools();
}

/** Flush Buffer queue. This discards all buffered but not processed frames */
void BUF_flush()
{
	pthread_mutex_lock(&mutex);
	LIST_concatenatePush(&pool, &queue);
	++stats.flushes;
	pthread_mutex_unlock(&mutex);
}

void BUF_flushUnlessHistoryEnabled()
{
	if (!cfgHistory)
		BUF_flush();
}

/** Get a new frame from the pool. Extend the pool if there are more dynamic
 *  pools to get. Discard oldest frame if there is no more dynamic pool
 * \return a frame which is about to be produced
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
	} else if (LIST_length(&dynPools) < dynMaxIncrements && createDynPool()) {
		/* pool was empty, but we just created another one */
#ifdef BUF_DEBUG
		LOG_logf(LOG_LEVEL_DEBUG, PFX, "Created another pool (%zu/%zu)",
			LIST_length(&dynPools), dynMaxPools);
#endif
		stats.discardedCur = 0;
		ret = LIST_itemSafe(&pool, LIST_shift(&pool), struct FrameLink, list);
	} else {
		/* no more space in the pool and no more pools
		 * -> sacrifice oldest frame */
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

/** Get a frame from the unused frame pool for the producer in order to fill it.
 * \return new frame
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

/** Commit a produced frame to the consumer queue.
 * \param frame filled frame to be delivered to the consumer
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

/** Abort producing a frame and return it to the pool.
 * \param frame aborted frame
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

/** Get a frame from the queue.
 * \return frame
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

/** Get a frame from the queue.
 * \param timeout_ms timeout in milliseconds
 * \return frame or NULL on timeout
 */
const struct OPENSKY_RawFrame * BUF_getFrameTimeout(uint_fast32_t timeout_ms)
{
	struct timespec ts;
	struct OPENSKY_RawFrame * ret;

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
			ret = NULL;
			break;
		} else if (r) {
			LOG_errno2(LOG_LEVEL_EMERG, r, PFX,
				"pthread_cond_timedwait failed");
		}
	}
	if (!LIST_empty(&queue)) {
		currentFrame = LIST_item(LIST_shift(&queue), struct FrameLink, list);
		ret = &currentFrame->frame;
	}
	CLEANUP_POP();

	return ret;
}

/** Put a frame back into the pool.
 * \note Called by the consumer after a frame is consumed in order to put the
 *   frame into the pool again.
 * \param frame frame to be put, must be the last one returned by one of
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

/** Put a frame back into the queue.
 * \note This is meant for unsuccessful behavior of the consumer. It should be
 *  called if the consumer wants to get that frame again next time when
 *  BUF_getFrame() or BUF_getFrameTimeout() is called.
 * \param frame frame to be put, must be the last one returned by one of
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
 * \param newPool pool to be deployed
 * \param size number of frames in that pool
 * \return true if deployment succeeded, false if allocation failed
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

	size_t i;
	for (i = 0; i < size; ++i)
		initFrames[i].pool = newPool;

	/* setup frame list */
	struct LIST_ListDC list = LIST_ListDC_INIT(list);
	LIST_fromVector(&list, initFrames, list, size);
	LIST_concatenatePush(&pool, &list);

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
	if (unlikely(!deployPool(newPool, cfgDynBacklog))) {
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

/** Collect unused frames from the overall pool to their own pool.
 * \note used for garbage collection
 */
static void collectPools()
{
	//struct FrameLink * frame, * next;
#ifdef BUF_DEBUG
	const size_t prevSize = LIST_length(&pool);
#endif

	/* iterate over all frames of the pool and keep a pointer to the
	 *  predecessor */
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
	LOG_logf(LOG_LEVEL_DEBUG, PFX, "Collected %zu of %zu frames from overall "
		"pool to their dynamic pools", prevSize - LIST_length(&pool), prevSize);
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

/** Free all fully collected pools.
 * \note used for garbage collection
 */
static void destroyUnusedPools()
{
	struct LIST_LinkD * l, * n;
#ifdef BUF_DEBUG
	const size_t prevSize = LIST_length(&dynPools);
#endif

	/* iterate over all dynamic pools */
	LIST_foreachSafe(&dynPools, l, n) {
		struct Pool * pool = LIST_item(l, struct Pool, list);
		if (likely(LIST_length(&pool->collect) ==  pool->size)) {
			/* fully collected pool */

			LIST_unlink(&dynPools, l);

			/* delete frames */
			free(pool->links);
			/* delete pool itself */
			free(pool);
		}
	}
#ifdef BUF_DEBUG
	LOG_logf(LOG_LEVEL_DEBUG, PFX, "Destroyed %zu of %zu dynamic pools",
		prevSize - LIST_length(&dynPools), prevSize);
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

