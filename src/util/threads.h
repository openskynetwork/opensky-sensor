/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_THREADS_H
#define _HAVE_THREADS_H

#include <pthread.h>
#include <stdbool.h>

#ifndef PTHREAD_CANCELLATION_POINTS
#include <time.h>
#include <errno.h>
#include <unistd.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CLEANUP_ROUTINES

#define CLEANUP_PUSH(fun, arg) \
	pthread_cleanup_push((void(*)(void*))(fun), (void*)(arg))

#define CLEANUP_POP() \
	pthread_cleanup_pop(1)

#define CLEANUP_POP0() \
	pthread_cleanup_pop(0)

#define CANCEL_DISABLE(r) \
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, r);

#define CANCEL_RESTORE(r) \
	pthread_setcancelstate(*r, NULL)

#define NOC_call(func, args...) ({ \
	int r; \
	CANCEL_DISABLE(&r); \
	__attribute__((unused)) __typeof__((func(args))) __ret = func(args); \
	CANCEL_RESTORE(&r); \
	__ret; \
})

#else

#define CLEANUP_PUSH(fun, arg) \
	do { \
		__attribute__((unused)) __typeof__(fun) _clean_fn = (fun); \
		__attribute__((unused)) __typeof__(arg) _clean_arg = (arg);

#define CLEANUP_POP() \
		_clean_fn(_clean_arg); \
	} while (0)

#define CLEANUP_POP0() \
	} while (0)

#define CANCEL_DISABLE(r) do { (void)r; } while(0)

#define CANCEL_RESTORE(r) do { (void)r; } while(0)

#define NOC_call(func, args...) func(args)

#endif

__attribute__((unused))
static void CLEANUP_UNLOCK(pthread_mutex_t * mutex)
{
	if (mutex)
		pthread_mutex_unlock(mutex);
}

#define CLEANUP_PUSH_LOCK(mutex) \
	CLEANUP_PUSH(&CLEANUP_UNLOCK, mutex); \
	pthread_mutex_lock(mutex);

#ifdef PTHREAD_CANCELLATION_POINTS

#define sleepCancelable(seconds) sleep(seconds)

#else

static inline unsigned int sleepCancelable(unsigned int seconds)
{
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	struct timespec ts;
	CLEANUP_PUSH_LOCK(&mutex);
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += seconds;
	while (true) {
		int rc = pthread_cond_timedwait(&cond, &mutex, &ts);
		if (rc == ETIMEDOUT)
			break;
		else if (rc != 0) {
			/* not very clean, but should not happen anyway */
			sleep(seconds);
			pthread_testcancel();
			break;
		}
	}
	CLEANUP_POP();
	return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif
