/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_LOG_H
#define _HAVE_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

enum LOG_LEVEL {
	LOG_LEVEL_INFO,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR
};

__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...);

void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str);

void LOG_flush();

__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...);

#ifdef __cplusplus
}
#endif

#endif
