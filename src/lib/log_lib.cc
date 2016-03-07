/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#include <log.h>
#include <iostream>
#include <stdio.h>
#include <stdarg.h>
#include <threads.h>
#include <util.h>
#include <stdlib.h>

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

void LOG_log(enum LOG_LEVEL level, const char * prefix, const char * str)
{
	int r;
	CANCEL_DISABLE(&r);
	std::cout << '[' << levelNames[level].name << ']';
	if (prefix)
		std::cout << ' ' << '[' << prefix << ']';
	std::cout << ' ' << str << std::endl;
	CANCEL_RESTORE(&r);

	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);
}

void LOG_flush()
{
	std::flush(std::cout);
}

__attribute__((format(printf, 3, 4)))
void LOG_errno(enum LOG_LEVEL level, const char * prefix, const char * fmt,
	...)
{
	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);
}

__attribute__((format(printf, 4, 5)))
void LOG_errno2(enum LOG_LEVEL level, int err, const char * prefix,
	const char * fmt, ...)
{
	if (unlikely(level == LOG_LEVEL_ERROR))
		exit(EXIT_FAILURE);
}
