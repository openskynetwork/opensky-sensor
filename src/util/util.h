/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_UTIL_H
#define _HAVE_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void UTIL_dropPrivileges();

#ifndef container_of
#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define ARRAY_SIZE(a) (sizeof ((a)) / sizeof (*(a)))

#define MIN(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); \
	_a < _b ? _a : _b; })

#define MAX(a,b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b); \
	_a < _b ? _b : _a; })

#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

uint32_t UTIL_randInt(uint32_t n);

#ifdef __cplusplus
}
#endif

#endif
