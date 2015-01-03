
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <buffer.h>
#include <stdio.h>

/** Linked Frame List */
struct FrameLink {
	/** Frame */
	struct ADSB_Frame frame;
	/** Next Element */
	struct FrameLink * next;
};

struct FrameList {
	struct FrameLink * head;
	struct FrameLink * tail;
	size_t size;
};

struct Pool {
	struct FrameLink * pool;
	struct Pool * next;
	size_t collect;
};

static struct Pool mainPool;

static size_t poolIncrement;
static size_t poolMaxSteps;
static size_t poolSteps = 0;
static struct Pool * pools = NULL;

static size_t poolSize = 0;
static volatile struct FrameList pool = { NULL, NULL };
static volatile struct FrameList queue = { NULL, NULL };

/** Currently filled frame (by writer), mainly for debugging purposes */
static struct FrameLink * newFrame = NULL;
/** Currently processed frame (by reader), for debugging purposes */
static struct FrameLink * currentFrame = NULL;

/** Frame Filter */
static uint8_t filter;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for listOut) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void deployPool(struct Pool * newPool, size_t size);

static inline struct FrameLink * shift(volatile struct FrameList * list);
static inline void unshift(volatile struct FrameList * list, struct FrameLink * frame);
static inline void push(volatile struct FrameList * list, struct FrameLink * frame);
static inline void append(volatile struct FrameList * list, struct FrameList * newList);

/** Initialize frame buffer.
 * \param initialBacklog number of empty frames for the steady state. Must be
 *  more than 2, should be about 10
 * \param increment number of empty frames to allocate additionally
 *  when the pool is empty
 * \param maxSteps max number of increments
 */
void BUF_init(size_t initialBacklog, size_t increment, size_t maxSteps)
{
	size_t i;

	assert (initialBacklog > 2);

	poolIncrement = increment;
	poolMaxSteps = maxSteps;

	deployPool(&mainPool, initialBacklog);
}

/** Set a filter: discard all other frames.
 * \param frameType frame type to accept or 0 to unset the filter
 */
void BUF_setFilter(uint8_t frameType)
{
	filter = frameType;
}

static void deployPool(struct Pool * newPool, size_t size)
{
	printf("BUF: deploying new pool (%d) of size %zu\n", poolSteps, size);

	struct FrameLink * initFrames = malloc(size * sizeof *initFrames);
	if (!initFrames)
		error(-1, errno, "malloc failed");

	newPool->pool = initFrames;
	newPool->next = NULL;
	newPool->collect = 0;

	struct FrameList list;
	list.head = initFrames;
	list.tail = initFrames + size - 1;
	size_t i;
	struct FrameLink * frame = initFrames;
	for (i = 0; i < size; ++i, ++frame)
		frame->next = frame + 1;
	list.tail->next = NULL;
	list.size = size;

	poolSize += size;
	append(&pool, &list);
}

static void createPool()
{
	struct Pool * newPool = malloc(sizeof *newPool);
	if (!newPool)
		error(-1, errno, "malloc failed");

	deployPool(newPool, poolIncrement);
	newPool->next = pools;
	pools = newPool;
}

static struct FrameLink * getFrame()
{
	if (pool.head)
		return shift(&pool);

	printf("BUF: Pool empty, checking for more pools (%zu to %zu)\n", poolSteps, poolMaxSteps);
	if (poolSteps < poolMaxSteps) {
		createPool();
		++poolSteps;
		assert (pool.head);
		return shift(&pool);
	}

	printf("BUF: sacrificing oldest frame\n");

	/* no more space in the pool and no more pools -> sacrifice oldest frame */
	printf("Test S1: %zu, %lu\n", queue.size, (intptr_t)queue.head);
	struct FrameLink * ret = shift(&queue);
	printf("Test S2: %zu, %lu\n", queue.size, (intptr_t)queue.head);
	return ret;
}

/** Get a frame from the unused frame pool for the writer in order to fill it.
 * \return new frame
 */
struct ADSB_Frame * BUF_newFrame()
{
	assert (!newFrame);
	pthread_mutex_lock(&mutex);
	newFrame = getFrame();
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
	if (filter && filter != frame->type) {
		unshift(&pool, newFrame);
	} else {
		push(&queue, newFrame);
		pthread_cond_broadcast(&cond);
	}
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
	unshift(&pool, currentFrame);
	//printf("Release: %zu, %lu\n", queue.size, (intptr_t)queue.head);
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
	//printf("Put: %zu, %lu\n", queue.size, (intptr_t)queue.head);
	pthread_mutex_unlock(&mutex);
	currentFrame = NULL;
}

/** Unqueue the first element of a list and return it.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail (if applicable)
 * \return first element of the list
 */
static inline struct FrameLink * shift(volatile struct FrameList * list)
{
	assert(!!list->head == !!list->tail);
	/*if (list == &queue)
		printf("S: Old List Head: %lu\n", (intptr_t)list->head);*/
	if (!list->head)
		return NULL;
	struct FrameLink * ret = list->head;
	list->head = ret->next;
	if (list->tail == ret)
		list->tail = NULL;
	ret->next = NULL;
	--list->size;
	/*if (list == &queue)
		printf("S: New List Head: %lu\n", (intptr_t)list->head);*/
	assert(!!list->head == !!list->tail);
	return ret;
}

/** Enqueue a new frame at front of a list.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail (if applicable)
 * \param frame the frame to be enqueued
 */
static inline void unshift(volatile struct FrameList * list, struct FrameLink * frame)
{
	assert(!!list->head == !!list->tail);
	/*if (list == &queue)
		printf("U: Old List Head: %lu\n", (intptr_t)list->head);*/
	frame->next = list->head;
	list->head = frame;
	if (!list->tail)
		list->tail = frame;
	/*if (list == &queue)
		printf("U: New List Head: %lu\n", (intptr_t)list->head);*/
	++list->size;
	assert(!!list->head == !!list->tail);
}

/** Enqueue a new frame at the end of a list.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail, must not be NULL
 * \param frame frame to be appended to the list
 */
static inline void push(volatile struct FrameList * list, struct FrameLink * frame)
{
	assert(!!list->head == !!list->tail);
	frame->next = NULL;
	/*if (list == &queue)
		printf("P: Old List Head: %lu\n", (intptr_t)list->head);*/
	if (!list->head) {
		list->head = list->tail = frame;
	} else {
		list->tail->next = frame;
		list->tail = frame;
	}
	/*if (list == &queue)
		printf("P: New List Head: %lu\n", (intptr_t)list->head);*/
	assert(!!list->head == !!list->tail);
	++list->size;
}

/** Append a new frame list at the end of a list.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail, must not be NULL
 * \param frame frame to be appended to the list
 */
static inline void append(volatile struct FrameList * list, struct FrameList * newList)
{
	assert(!!list->head == !!list->tail);
	if (!list->head)
		list->head = newList->head;
	else
		list->tail->next = newList->head;
	list->tail = newList->tail;
	list->size += newList->size;
	assert(!!list->head == !!list->tail);
}
