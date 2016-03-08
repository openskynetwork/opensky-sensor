/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#include <log.h>
#include <opensky.hh>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <threads.h>
#include <util.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

namespace OpenSky {

static std::ostream * msgLog = &std::cout;
static std::ostream * errLog = &std::cerr;

__attribute__((visibility("default")))
void setLogStreams(std::ostream & msgLog, std::ostream & errLog)
{
	OpenSky::msgLog = &msgLog;
	OpenSky::errLog = &errLog;
}

}

struct LevelName {
	const char name[6];
	size_t len;
};

static const struct LevelName levelNames[] = {
	/* [LOG_LEVEL_INFO] = */ { "INFO", 4 },
	/* [LOG_LEVEL_DEBUG] = */ { "DEBUG", 5 },
	/* [LOG_LEVEL_WARN] = */ { "WARN", 6 },
	/* [LOG_LEVEL_ERROR] = */ { "ERROR", 5 }
};

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

static std::ostream & getStream(enum LOG_LEVEL level)
{
	if (unlikely(level == LOG_LEVEL_ERROR))
		return *OpenSky::errLog;
	else
		return *OpenSky::msgLog;
}

void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str)
{
	int r;
	CANCEL_DISABLE(&r);
	std::ostream & stream = getStream(level);
	stream << '[' << levelNames[level].name << ']';
	if (prefix)
		stream << ' ' << '[' << prefix << ']';
	stream << ' ' << str << std::endl;
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR)) {
		LOG_flush();
		exit(EXIT_FAILURE);
	}
}

void LOG_flush()
{
	std::flush(*OpenSky::msgLog);
	std::flush(*OpenSky::errLog);
}

static void logWithErr(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, va_list ap)
{
	char str[2048];
	vsnprintf(str, sizeof str, fmt, ap);

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

	int r;
	CANCEL_DISABLE(&r);

	std::ostream & stream = getStream(level);
	stream << '[' << levelNames[level].name << ']';
	if (prefix)
		stream << ' ' << '[' << prefix << ']';
	stream << ' ' << str;

	if (errstr)
		stream << ':' << errstr << ' ' << '(' << err << ')' << std::endl;
	else
		stream << ": Unknown error (" << err << ')' << std::endl;

	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR)) {
		LOG_flush();
		exit(EXIT_FAILURE);
	}
}

__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...)
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
