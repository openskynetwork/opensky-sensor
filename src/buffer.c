
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <buffer.h>

static struct ADSB_Frame * listIn = NULL;
static struct ADSB_Frame * listOut = NULL;
static struct ADSB_Frame * listOutEnd = NULL;
static struct ADSB_Frame * processingIn = NULL;
static struct ADSB_Frame * processingOut = NULL;

static pthread_mutex_t mutexIn = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutexOut = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condOut = PTHREAD_COND_INITIALIZER;

static inline struct ADSB_Frame * shift(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail);
static inline void unshift(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail, struct ADSB_Frame * frame);
static inline void push(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail, struct ADSB_Frame * frame);

void FB_init(size_t backlog)
{
	size_t i;

	assert (backlog > 2);

	for (i = 0; i < backlog; ++i) {
		struct ADSB_Frame * frame = malloc(sizeof *frame);
		if (!frame)
			error(-1, errno, "malloc failed");
		frame->next = listIn;
		listIn = frame;
	}
}

struct ADSB_Frame * FB_new()
{
	assert (!processingIn);
	if (listIn) {
		pthread_mutex_lock(&mutexIn);
		processingIn = shift(&listIn, NULL);
		pthread_mutex_unlock(&mutexIn);
	} else {
		pthread_mutex_lock(&mutexOut);
		processingIn = shift(&listOut, &listOutEnd);
		pthread_mutex_unlock(&mutexOut);
	}
	assert (processingIn);
	return processingIn;
}

void FB_put(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (processingIn == frame);

	pthread_mutex_lock(&mutexOut);
	push(&listOut, &listOutEnd, frame);
	pthread_cond_broadcast(&condOut);
	pthread_mutex_unlock(&mutexOut);
	processingIn = NULL;
}

struct ADSB_Frame * FB_get()
{
	if (processingOut)
		FB_done(processingOut);

	pthread_mutex_lock(&mutexOut);
	while (!listOut)
		pthread_cond_wait(&condOut, &mutexOut);
	processingOut = shift(&listOut, &listOutEnd);
	pthread_mutex_unlock(&mutexOut);

	return processingOut;
}

void FB_done(struct ADSB_Frame * frame)
{
	assert (frame);
	assert (frame == processingOut);

	pthread_mutex_lock(&mutexIn);
	unshift(&listIn, NULL, processingOut);
	pthread_mutex_unlock(&mutexIn);
	processingOut = NULL;
}

static inline struct ADSB_Frame * shift(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail)
{
	if (!*listHead)
		return NULL;
	struct ADSB_Frame * ret = *listHead;
	*listHead = ret->next;
	if (listTail && *listTail == ret)
		*listTail = NULL;
	ret->next = NULL;
	return ret;
}

static inline void unshift(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail, struct ADSB_Frame * frame)
{
	frame->next = *listHead;
	*listHead = frame;
	if (listTail && !*listTail)
		*listTail = frame;
}

static inline void push(struct ADSB_Frame ** listHead,
	struct ADSB_Frame ** listTail, struct ADSB_Frame * frame)
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

