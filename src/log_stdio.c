/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#include <log.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

struct LevelName {
	const char name[7];
	size_t len;
};

static const struct LevelName levelNames[] = {
	[LOG_LEVEL_INFO] = { "INFO", 4 },
	[LOG_LEVEL_DEBUG] = { "DEBUG", 5 },
	[LOG_LEVEL_SEVERE] = { "SEVERE", 6 },
	[LOG_LEVEL_ERROR] = { "ERROR", 5 }
};

__attribute__((format(printf, 3, 4)))
void LOG_logf(enum LOG_LEVEL level, const char * prefix, const char * fmt, ...)
{
	const struct LevelName * levelName = &levelNames[level];

	printf("[%s] ", levelName->name);
	if (prefix)
		printf("[%s] ", prefix);

	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');

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

	printf("[%s] ", levelName->name);
	if (prefix)
		printf("[%s] ", prefix);

	puts(str);
}
