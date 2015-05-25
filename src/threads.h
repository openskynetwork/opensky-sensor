#ifndef _HAVE_THREADS_h
#define _HAVE_THREADS_h
#endif

#include <pthread.h>

#define CLEANUP_PUSH(fun, arg) \
	pthread_cleanup_push(fun, arg)

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
	__attribute__((unused)) __typeof__((func(args))) ret = func(args); \
	CANCEL_RESTORE(&r); \
	r; \
})

#define NOC_printf(format, args...) NOC_call(printf, format, args)

#define NOC_fprintf(f, format, args...) NOC_call(fprintf, f, format, args)

#define NOC_puts(str) NOC_call(puts, str)
