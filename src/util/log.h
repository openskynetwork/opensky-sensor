/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_LOG_H
#define _HAVE_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/** Log Levels */
enum LOG_LEVEL {
	/** Just an information */
	LOG_LEVEL_INFO,
	/** Debugging message */
	LOG_LEVEL_DEBUG,
	/** Warning */
	LOG_LEVEL_WARN,
	/** Recoverable error */
	LOG_LEVEL_ERROR,
	/** Unrecoverable error. This will cause the process to abort */
	LOG_LEVEL_EMERG
};

__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...);

void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str);

void LOG_flush();

__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...);

__attribute__((format(printf, 4, 5)))
void LOG_errno2(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
