/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_UTIL_H
#define _HAVE_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

bool UTIL_getSerial(uint32_t * serial);

#ifndef container_of
#define container_of(ptr, type, member) ({ \
	const typeof( ((type *)0)->member ) *__mptr = (ptr); \
	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

#endif
