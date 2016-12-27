/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_ENDEC_H
#define _HAVE_ENDEC_H

#include <stdint.h>
#include <string.h>
#include <endian.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Decode an unsigned 16 bit integer.
 * @param buf input buffer, expected to be (at least) 2 byte
 * @return 16 bit unsigned integer represented by the buffer
 */
static inline uint_fast16_t ENDEC_tou16(const uint8_t * buf)
{
	uint16_t u;
	memcpy(&u, buf, sizeof u);
	return be16toh(u);
}

/** Decode a signed 16 bit integer.
 * @param buf input buffer, expected to be (at least) 2 byte
 * @return 16 bit signed integer represented by the buffer
 */
static inline int_fast16_t ENDEC_toi16(const uint8_t * buf)
{
	union {
		uint16_t u;
		int16_t i;
	} c;
	c.u = ENDEC_tou16(buf);
	return c.i;
}

/** Decode an unsigned 32 bit integer.
 * @param buf input buffer, expected to be (at least) 4 byte
 * @return 32 bit unsigned integer represented by the buffer
 */
static inline uint_fast32_t ENDEC_tou32(const uint8_t * buf)
{
	uint32_t u;
	memcpy(&u, buf, sizeof u);
	return be32toh(u);
}


/** Decode a signed 32 bit integer.
 * @param buf input buffer, expected to be (at least) 4 byte
 * @return 32 bit signed integer represented by the buffer
 */
static inline int_fast32_t ENDEC_toi32(const uint8_t * buf)
{
	union {
		int32_t i;
		uint32_t u;
	} c;
	c.u = ENDEC_tou32(buf);
	return c.i;
}


/** Decode an unsigned 64 bit integer.
 * @param buf input buffer, expected to be (at least) 8 byte
 * @return 64 bit signed integer represented by the buffer
 */
static inline uint_fast64_t ENDEC_tou64(const uint8_t * buf)
{
	uint64_t u;
	memcpy(&u, buf, sizeof u);
	return be64toh(u);
}

/** Decode an IEEE 754 single precision floating point number.
 * @param buf input buffer, expected to be (at least) 4 byte
 * @return floating point number represented by the buffer
 */
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

/** Decode an IEEE 754 double precision floating point number.
 * @param buf input buffer, expected to be (at least) 8 byte
 * @return floating point number represented by the buffer
 */
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

/** Encode a 32 bit unsigned integer.
 * @param u unsigned integer to be encoded
 * @param buf buffer to write into, expected to be (at least) 4 byte
 */
static inline void ENDEC_fromu32(uint32_t u, uint8_t * buf)
{
	u = htobe32(u);
	memcpy(buf, &u, sizeof u);
}

/** Encode a 64 bit unsigned integer.
 * @param u unsigned integer to be encoded
 * @param buf buffer to write into, expected to be (at least) 8 byte
 */
static inline void ENDEC_fromu64(uint64_t u, uint8_t * buf)
{
	u = htobe64(u);
	memcpy(buf, &u, sizeof u);
}

/** Encode an IEEE 754 double precision floating point number.
 * @param d floating point number to be encoded
 * @param buf buffer to write into, expected to be (at least) 8 byte
 */
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
