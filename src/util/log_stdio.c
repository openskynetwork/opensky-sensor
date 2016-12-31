/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "log.h"
#include "threads.h"
#include "util.h"

/** Log level Names */
static const char * levelNames[] = {
	[LOG_LEVEL_INFO] = "INFO",
	[LOG_LEVEL_DEBUG] = "DEBUG",
	[LOG_LEVEL_WARN] = "WARN",
	[LOG_LEVEL_ERROR] = "ERROR",
	[LOG_LEVEL_EMERG] = "EMERG"
};

/** Mutex: only one thread can log to stdout */
static pthread_mutex_t stdioMutex = PTHREAD_MUTEX_INITIALIZER;

/** Print log message with printf-like formatting.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param prefix Component name
 * @param fmt printf format string
 */
__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	int r;
	CANCEL_DISABLE(&r);
	pthread_mutex_lock(&stdioMutex);

	printf("[%s] ", levelNames[level]);
	if (prefix)
		printf("[%s] ", prefix);

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');

	pthread_mutex_unlock(&stdioMutex);
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_EMERG)) {
		LOG_flush();
		abort();
	}
}

/** Print log message.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param prefix Component name
 * @param str log message
 */
void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str)
{
	int r;
	CANCEL_DISABLE(&r);
	pthread_mutex_lock(&stdioMutex);

	printf("[%s] ", levelNames[level]);
	if (prefix)
		printf("[%s] ", prefix);

	puts(str);

	pthread_mutex_unlock(&stdioMutex);
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_EMERG)) {
		LOG_flush();
		abort();
	}
}

/** Print log message with error code.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param err error code
 * @param prefix Component name
 * @param fmt format string
 * @param ap argument list for format
 */
static void logWithErr(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, va_list ap)
{
	int r;
	CANCEL_DISABLE(&r);
	pthread_mutex_lock(&stdioMutex);

	printf("[%s] ", levelNames[level]);
	if (prefix)
		printf("[%s] ", prefix);

	vprintf(fmt, ap);

	char * errstr;
#ifdef HAVE_STRERROR_R
	char errbuf[100];
#ifdef STRERROR_R_CHAR_P
	errstr = strerror_r(err, errbuf, sizeof errbuf);
#else
	int rc = strerror_r(err, errbuf, sizeof errbuf);
	if (rc == 0)
		errstr = errbuf;
	else
		errstr = NULL;
#endif
#else
	errstr = strerror(err);
#endif

	if (errstr)
		printf(": %s (%d)", errstr, err);
	else
		printf(": Unknown Error (%d)", err);
	putchar('\n');

	pthread_mutex_unlock(&stdioMutex);
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_EMERG)) {
		LOG_flush();
		abort();
	}
}

/** Print log message with errno.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param prefix Component name
 * @param fmt format string
 */
__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	logWithErr(level, errno, prefix, fmt, ap);
	va_end(ap);
}

/** Print log message with error code.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param err error code
 * @param prefix Component name
 * @param fmt format string
 */
__attribute__((format(printf, 4, 5)))
void LOG_errno2(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	logWithErr(level, err, prefix, fmt, ap);
	va_end(ap);
}

/** Flush the log */
void LOG_flush()
{
	NOC_call(fflush, stdout);
}
