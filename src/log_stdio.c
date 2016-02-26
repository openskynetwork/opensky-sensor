/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#include <log.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <threads.h>
#include <util.h>

struct LevelName {
	const char name[6];
	size_t len;
};

static const struct LevelName levelNames[] = {
	[LOG_LEVEL_INFO] = { "INFO", 4 },
	[LOG_LEVEL_DEBUG] = { "DEBUG", 5 },
	[LOG_LEVEL_WARN] = { "WARN", 6 },
	[LOG_LEVEL_ERROR] = { "ERROR", 5 }
};

__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	const struct LevelName * levelName = &levelNames[level];

	int r;
	CANCEL_DISABLE(&r);

	printf("[%s] ", levelName->name);
	if (prefix)
		printf("[%s] ", prefix);

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');

	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);

#if 0
	char buf[1000];
	char * bufptr;
	size_t len = sizeof buf;

	buf[0] = '[';
	struct LevelName * levelName = &levelNames[level];
	memcpy(buf + 1, levelName->name, levelName->len);
	bufptr = buf + 1 + levelName;
	*bufptr++ = ']';
	*bufptr++ = ' ';

	if (prefix && *prefix) {
		*bufptr++ = '[';
		len -= bufptr - buf;
		strncpy(bufptr, prefix, len);
		bufptr +=
	}


	va_list ap;
	va_start(ap, fmt);
	size_t len = vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
#endif
}

void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str)
{
	const struct LevelName * levelName = &levelNames[level];

	int r;
	CANCEL_DISABLE(&r);

	printf("[%s] ", levelName->name);
	if (prefix)
		printf("[%s] ", prefix);

	puts(str);

	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);
}

static void logWithErr(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, va_list ap)
{
	const struct LevelName * levelName = &levelNames[level];

	int r;
	CANCEL_DISABLE(&r);

	printf("[%s] ", levelName->name);
	if (prefix)
		printf("[%s] ", prefix);

	vprintf(fmt, ap);

	char errstr[100];
	int rc = strerror_r(err, errstr, sizeof errstr);
	if (rc == 0)
		printf(": %s (%d)", errstr, errno);
	else
		printf(": ?? (%d)", errno);
	putchar('\n');

	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);
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
