/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "log.h"
#include "threads.h"
#include "util.h"

static const char * levelNames[] = {
	[LOG_LEVEL_INFO] = "INFO",
	[LOG_LEVEL_DEBUG] = "DEBUG",
	[LOG_LEVEL_WARN] = "WARN",
	[LOG_LEVEL_ERROR] = "ERROR",
	[LOG_LEVEL_EMERG] = "EMERG"
};

static pthread_mutex_t stdioMutex = PTHREAD_MUTEX_INITIALIZER;

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

	char errbuf[100];
	char * errstr;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	int rc = strerror_r(err, errbuf, sizeof errbuf);
	if (rc == 0)
		errstr = errbuf;
	else
		errstr = NULL;
#else
	errstr = strerror_r(err, errbuf, sizeof errbuf);
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

__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	logWithErr(level, errno, prefix, fmt, ap);
	va_end(ap);
}

__attribute__((format(printf, 4, 5)))
void LOG_errno2(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	logWithErr(level, err, prefix, fmt, ap);
	va_end(ap);
}

void LOG_flush()
{
	NOC_call(fflush, stdout);
}
