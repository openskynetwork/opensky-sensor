
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <buffer.h>

/** Linked Frame List */
struct FB_FrameList {
	/** Frame */
	struct ADSB_Frame frame;
	/** Next Element */
	struct FB_FrameList * next;
};

/** Pool of unused frames, can be used by the writer */
static struct FB_FrameList * listIn = NULL;
/** List for used frames, to be read by the reader */
static struct FB_FrameList * listOut = NULL;
/** End of List for used frames, in order to speed up push operation */
static struct FB_FrameList * listOutEnd = NULL;
/** Currently filled frame (by writer), mainly for debugging purposes */
static struct FB_FrameList * processingIn = NULL;
/** Currently processed frame (by reader), for debugging purposes */
static struct FB_FrameList * processingOut = NULL;

/** Mutex */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/** Reader condition (for listOut) */
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static inline struct FB_FrameList * shift(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail);
static inline void unshift(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail, struct FB_FrameList * frame);
static inline void push(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail, struct FB_FrameList * frame);

/** Initialize frame buffer.
 * \param backlog Max number of frames in buffer before discarding the oldest
 *  one. Must be at least 2.
 */
void FB_init(size_t backlog)
{
	size_t i;

	assert (backlog > 2);

	for (i = 0; i < backlog; ++i) {
		struct FB_FrameList * frame = malloc(sizeof *frame);
		if (!frame)
			error(-1, errno, "malloc failed");
		frame->next = listIn;
		listIn = frame;
	}
}

/** Get a frame from the unused frame pool for the writer in order to fill it.
 * \return new frame
 */
struct ADSB_Frame * FB_new()
{
	assert (!processingIn);
	pthread_mutex_lock(&mutex);
	if (listIn) /* pool is not empty, get a frame */
		processingIn = shift(&listIn, NULL);
	else /* pool is empty, sacrifice oldest frame */
		processingIn = shift(&listOut, &listOutEnd);
	pthread_mutex_unlock(&mutex);
	assert (processingIn);
	return &processingIn->frame;
}

/** Put a filled frame from writer to reader queue.
 * \param frame filled frame to be delivered to the reader
 */
void FB_put(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (&processingIn->frame == frame);

	pthread_mutex_lock(&mutex);
	push(&listOut, &listOutEnd, processingIn);
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
	processingIn = NULL;
}

/** Get a frame from the queue.
 * \return adsb frame
 */
struct ADSB_Frame * FB_get(int32_t timeout_ms)
{
	struct timespec ts;

	if (timeout_ms >= 0) {
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += timeout_ms / 1000;
		ts.tv_nsec += (timeout_ms % 1000) * 1000000;
		if (ts.tv_nsec >= 1000000000) {
			++ts.tv_sec;
			ts.tv_nsec -= 1000000000;
		}
	}

	if (processingOut)
		FB_done(&processingOut->frame);

	pthread_mutex_lock(&mutex);
	while (!listOut) {
		int r;
		if (timeout_ms < 0)
			r = pthread_cond_wait(&cond, &mutex);
		else {
			r = pthread_cond_timedwait(&cond, &mutex, &ts);
			if (r == ETIMEDOUT) {
				pthread_mutex_unlock(&mutex);
				return NULL;
			}
		}
		if (r)
			error(-1, r, "pthread_cond_wait failed");
	}
	processingOut = shift(&listOut, &listOutEnd);
	pthread_mutex_unlock(&mutex);

	return &processingOut->frame;
}

/** Put a frame back into the pool.
 * \note Can be called by the reader after a frame is processed in order to
 *  put the frame into the pool again. If it is not called explicitly, it is
 *  called by the next call of FB_get implicitly.
 */
void FB_done(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (frame == &processingOut->frame);

	pthread_mutex_lock(&mutex);
	unshift(&listIn, NULL, processingOut);
	pthread_mutex_unlock(&mutex);
	processingOut = NULL;
}

/** Unqueue the first element of a list and return it.
 * \param listHead pointer to list head
 * \param listTail pointer to list tail (if applicable)
 * \return first element of the list
 */
static inline struct FB_FrameList * shift(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail)
{
	if (!*listHead)
		return NULL;
	struct FB_FrameList * ret = *listHead;
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
static inline void unshift(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail, struct FB_FrameList * frame)
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
static inline void push(struct FB_FrameList ** listHead,
	struct FB_FrameList ** listTail, struct FB_FrameList * frame)
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

