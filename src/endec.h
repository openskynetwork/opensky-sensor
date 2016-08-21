/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_ENDEC_H
#define _HAVE_ENDEC_H

#include <stdint.h>
#include <string.h>
#include <endian.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint_fast16_t ENDEC_tou16(const uint8_t * buf)
{
	uint16_t u;
	memcpy(&u, buf, sizeof u);
	return be16toh(u);
}

static inline int_fast16_t ENDEC_toi16(const uint8_t * buf)
{
	union {
		uint16_t u;
		int16_t i;
	} c;
	c.u = ENDEC_tou16(buf);
	return c.i;
}

static inline uint_fast32_t ENDEC_tou32(const uint8_t * buf)
{
	uint32_t u;
	memcpy(&u, buf, sizeof u);
	return be32toh(u);
}

static inline int_fast32_t ENDEC_toi32(const uint8_t * buf)
{
	union {
		int32_t i;
		uint32_t u;
	} c;
	c.u = ENDEC_tou32(buf);
	return c.i;
}

static inline uint_fast64_t ENDEC_tou64(const uint8_t * buf)
{
	uint64_t u;
	memcpy(&u, buf, sizeof u);
	return be64toh(u);
}

static inline float ENDEC_tofloat(const uint8_t * buf)
{
	union
	{
		uint32_t u;
		float f;
	} c;
	c.u = ENDEC_tou32(buf);
	return c.f;
}

static inline double ENDEC_todouble(const uint8_t * buf)
{
	union
	{
		uint64_t u;
		double d;
	} c;
	c.u = ENDEC_tou64(buf);
	return c.d;
}

static inline void ENDEC_fromu64(uint_fast64_t u, uint8_t * buf)
{
	u = be64toh(u);
	memcpy(buf, &u, sizeof u);
}

static inline void ENDEC_fromdouble(double d, uint8_t * buf)
{
	union
	{
		uint64_t u;
		double d;
	} c;
	c.d = d;
	ENDEC_fromu64(c.u, buf);
}

#ifdef __cplusplus
}
#endif

#endif
