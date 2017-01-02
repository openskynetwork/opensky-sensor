/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "opensky.hh"
#include "util/log.h"
#include "util/threads.h"
#include "util/util.h"
#include "util/port/socket.h"

namespace OpenSky {

/** Message log stream */
static std::ostream * msgLog = &std::cout;
/** Error log stream */
static std::ostream * errLog = &std::cerr;

__attribute__((visibility("default")))
void setLogStreams(std::ostream & msgLog, std::ostream & errLog)
{
	OpenSky::msgLog = &msgLog;
	OpenSky::errLog = &errLog;
}

}

/** Mutex: only one thread can log */
static pthread_mutex_t logMutex = PTHREAD_MUTEX_INITIALIZER;

/** Log level prefixes */
static const char * levelNames[] = {
	/* [LOG_LEVEL_INFO] = */ "INFO",
	/* [LOG_LEVEL_DEBUG] = */ "DEBUG",
	/* [LOG_LEVEL_WARN] = */ "WARN",
	/* [LOG_LEVEL_ERROR] = */ "ERROR",
	/* [LOG_LEVEL_EMERG] = */ "EMERG"
};

/** Print log message with printf-like formatting.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param prefix Component name
 * @param fmt printf format string
 */
__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	char str[2048];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(str, sizeof str, fmt, ap);
	va_end(ap);
	LOG_log(level, prefix, str);
}

/** Get log output stream by log level */
static std::ostream & getStream(enum LOG_LEVEL level)
{
	if (unlikely(level == LOG_LEVEL_ERROR || level == LOG_LEVEL_EMERG))
		return *OpenSky::errLog;
	else
		return *OpenSky::msgLog;
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
	pthread_mutex_lock(&logMutex);

	std::ostream & stream = getStream(level);
	stream << '[' << levelNames[level] << ']';
	if (prefix)
		stream << ' ' << '[' << prefix << ']';
	stream << ' ' << str << std::endl;

	pthread_mutex_unlock(&logMutex);
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_EMERG)) {
		LOG_flush();
		abort();
	}
}

/** Flush the log */
void LOG_flush()
{
	int r;
	CANCEL_DISABLE(&r);
	std::flush(*OpenSky::msgLog);
	std::flush(*OpenSky::errLog);
	CANCEL_RESTORE(&r);
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
	char str[2048];
	vsnprintf(str, sizeof str, fmt, ap);

	char * errstr;
#ifdef HAVE_STRERROR_R
	char errbuf[100];
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
	int rc = strerror_r(err, errbuf, sizeof errbuf);
	if (rc == 0)
		errstr = errbuf;
	else
		errstr = NULL;
#else
	errstr = strerror_r(err, errbuf, sizeof errbuf);
#endif
#else
	errstr = strerror(err);
#endif

	int r;
	CANCEL_DISABLE(&r);
	pthread_mutex_lock(&logMutex);

	std::ostream & stream = getStream(level);
	stream << '[' << levelNames[level] << ']';
	if (prefix)
		stream << ' ' << '[' << prefix << ']';
	stream << ' ' << str;

	if (errstr)
		stream << ':' << errstr << ' ' << '(' << err << ')' << std::endl;
	else
		stream << ": Unknown error (" << err << ')' << std::endl;

	pthread_mutex_unlock(&logMutex);
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
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...)
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

/** Print log message with error of network.
 * @param level Log level. If it is LOG_LEVEL_EMERG, the function will abort
 *  the process.
 * @param prefix Component name
 * @param fmt format string
 */
__attribute__((format(printf, 3, 4)))
void LOG_errnet(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...)
{
	va_list ap;
	va_start(ap, fmt);
#ifdef HAVE_SYS_SOCKET_H
	logWithErr(level, errno, prefix, fmt, ap);
#else
	int err = SOCK_getError();

	char str[2048];
	vsnprintf(str, sizeof str, fmt, ap);

	int r;
	CANCEL_DISABLE(&r);
	pthread_mutex_lock(&logMutex);

	LPSTR errstr = NULL;
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		0, err, 0, (LPSTR)&errstr, 0, 0);

	std::ostream & stream = getStream(level);
	stream << '[' << levelNames[level] << ']';
	if (prefix)
		stream << ' ' << '[' << prefix << ']';
	stream << ' ' << str;

	if (errstr) {
		size_t len = strlen(errstr);
		if (len >= 2)
			errstr[len - 2] = '\0';
		stream << ':' << errstr << ' ' << '(' << err << ')' << std::endl;
		LocalFree(errstr);
	} else {
		stream << ": Unknown error (" << err << ')' << std::endl;
	}

	pthread_mutex_unlock(&logMutex);
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_EMERG)) {
		LOG_flush();
		abort();
	}
#endif
	va_end(ap);
}
