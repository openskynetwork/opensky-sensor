
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <buffer.h>

/** Linked Frame List */
struct BUF_FrameList {
	/** Frame */
	struct ADSB_Frame frame;
	/** Next Element */
	struct BUF_FrameList * next;
};

/** Pool of unused frames, can be used by the writer */
static struct BUF_FrameList * volatile pool = NULL;
/** List for used frames, to be read by the reader */
static struct BUF_FrameList * volatile queueHead = NULL;
/** End of List for used frames, in order to speed up push operation */
static struct BUF_FrameList * volatile queueTail = NULL;
/** Currently filled frame (by writer), mainly for debugging purposes */
static struct BUF_FrameList * newFrame = NULL;
/** Currently processed frame (by reader), for debugging purposes */
static struct BUF_FrameList * currentFrame = NULL;

/** Frame Filter */
static uint8_t filter;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for listOut) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static inline struct BUF_FrameList * shift(
	struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail);
static inline void unshift(struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail, struct BUF_FrameList * frame);
static inline void push(struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail, struct BUF_FrameList * frame);

/** Initialize frame buffer.
 * \param backlog Max number of frames in buffer before discarding the oldest
 *  one. Must be at least 2.
 */
void BUF_init(size_t backlog)
{
	size_t i;

	assert (backlog > 2);

	for (i = 0; i < backlog; ++i) {
		struct BUF_FrameList * frame = malloc(sizeof *frame);
		if (!frame)
			error(-1, errno, "malloc failed");
		frame->next = pool;
		pool = frame;
	}
}

/** Set a filter: discard all other frames.
 * \param frameType frame type to accept or 0 to unset the filter
 */
void BUF_setFilter(uint8_t frameType)
{
	filter = frameType;
}

/** Get a frame from the unused frame pool for the writer in order to fill it.
 * \return new frame
 */
struct ADSB_Frame * BUF_newFrame()
{
	assert (!newFrame);
	pthread_mutex_lock(&mutex);
	if (pool) /* pool is not empty, get a frame */
		newFrame = shift(&pool, NULL);
	else /* pool is empty, sacrifice oldest frame */
		newFrame = shift(&queueHead, &queueTail);
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
		unshift(&pool, NULL, newFrame);
	} else {
		push(&queueHead, &queueTail, newFrame);
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
	while (!queueHead) {
		int r = pthread_cond_wait(&cond, &mutex);
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	}
	currentFrame = shift(&queueHead, &queueTail);
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
	while (!queueHead) {
		int r = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (r == ETIMEDOUT) {
			pthread_mutex_unlock(&mutex);
			return NULL;
		} else if (r) {
			error(-1, r, "pthread_cond_wait failed");
		}
	}
	currentFrame = shift(&queueHead, &queueTail);
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
	unshift(&pool, NULL, currentFrame);
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
	unshift(&queueHead, &queueTail, currentFrame);
	pthread_mutex_unlock(&mutex);
	currentFrame = NULL;
}

/** Unqueue the first element of a list and return it.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail (if applicable)
 * \return first element of the list
 */
static inline struct BUF_FrameList * shift(
	struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail)
{
	if (!*listHead)
		return NULL;
	struct BUF_FrameList * ret = *listHead;
	*listHead = ret->next;
	if (listTail && *listTail == ret)
		*listTail = NULL;
	ret->next = NULL;
	return ret;
}

/** Enqueue a new frame at front of a list.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail (if applicable)
 * \param frame the frame to be enqueued
 */
static inline void unshift(struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail, struct BUF_FrameList * frame)
{
	frame->next = *listHead;
	*listHead = frame;
	if (listTail && !*listTail)
		*listTail = frame;
}

/** Enqueue a new frame at the end of a list.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail, must not be NULL
 * \param frame frame to be appended to the list
 */
static inline void push(struct BUF_FrameList * volatile * listHead,
	struct BUF_FrameList * volatile * listTail, struct BUF_FrameList * frame)
{
	assert (listTail);
	frame->next = NULL;
	if (!*listTail)
		*listHead = *listTail = frame;
	else {
		(*listTail)->next = frame;
		if (!*listHead)
			*listHead = frame;
	}
}

