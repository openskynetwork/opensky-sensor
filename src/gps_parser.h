/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_GPS_PARSER_H
#define _HAVE_GPS_PARSER_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>

void GPS_PARSER_init();
void GPS_PARSER_destruct();

void GPS_PARSER_connect();
size_t GPS_PARSER_getFrame(uint8_t * buf, size_t bufLen);

static inline uint_fast16_t GPS_tou16(const uint8_t * buf)
{
	uint16_t u;
	memcpy(&u, buf, sizeof u);
	return be16toh(u);
}

static inline int_fast16_t GPS_toi16(const uint8_t * buf)
{
	union {
		uint16_t u;
		int16_t i;
	} c;
	c.u = GPS_tou16(buf);
	return c.i;
}

static inline uint_fast32_t GPS_tou32(const uint8_t * buf)
{
	uint32_t u;
	memcpy(&u, buf, sizeof u);
	return be32toh(u);
}

static inline int_fast32_t GPS_toi32(const uint8_t * buf)
{
	union {
		int32_t i;
		uint32_t u;
	} c;
	c.u = GPS_tou32(buf);
	return c.i;
}

static inline uint_fast64_t GPS_tou64(const uint8_t * buf)
{
	uint64_t u;
	memcpy(&u, buf, sizeof u);
	return be64toh(u);
}

static inline float GPS_tofloat(const uint8_t * buf)
{
	union
	{
		uint32_t u;
		float f;
	} c;
	c.u = GPS_tou32(buf);
	return c.f;
}

static inline double GPS_todouble(const uint8_t * buf)
{
	union
	{
		uint64_t u;
		double d;
	} c;
	c.u = GPS_tou64(buf);
	return c.d;
}

#endif
