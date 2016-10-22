/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_THREADS_H
#define _HAVE_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif

/* TODO: get rid of NOC_printf/... functions -> should be LOG now! */

#ifdef CLEANUP_ROUTINES

#include <pthread.h>

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

#define NOC_printf(format, args...) NOC_call(printf, format, args)

#define NOC_fprintf(f, format, args...) NOC_call(fprintf, f, format, args)

#define NOC_puts(str) NOC_call(puts, str)

#ifdef __cplusplus
}
#endif

#endif