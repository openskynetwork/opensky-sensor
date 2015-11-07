/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gps_parser.h>
#include <gps_input.h>
#include <util.h>
#include <assert.h>
#include <stdio.h>
#include <threads.h>

enum DECODE_STATUS {
	DECODE_STATUS_OK,
	DECODE_STATUS_RESYNC,
	DECODE_STATUS_CONNFAIL,
	DECODE_STATUS_BUFFER_FULL
};

/** receive buffer */
static uint8_t buf[128];
/** current pointer into buffer */
static uint8_t * bufCur;
/** buffer end */
static uint8_t * bufEnd;

static inline enum DECODE_STATUS decode(uint8_t * dst, size_t * len);
static inline bool discardAndFill();
static inline bool next(uint8_t * ch);
static inline bool synchronize();

void GPS_PARSER_init()
{
	GPS_INPUT_init();
}

void GPS_PARSER_destruct()
{
	GPS_INPUT_destruct();
}

/** Setup GPS receiver with some options. */
static bool configure()
{
/*#ifndef NETWORK
	const struct CFG_GPS * cfg = &CFG_config.gps;
#else
	return true;
#endif*/
	const uint8_t setutc[] = { 0x10, 0x35, 0x04, 0x00, 0x01, 0x08, 0x10, 0x03 };
	return GPS_INPUT_write(setutc, sizeof setutc) == sizeof setutc;
}

void GPS_PARSER_connect()
{
	while (true) {
		/* connect with input */
		GPS_INPUT_connect();

		/* configure input */
		if (configure())
			break;
	}

	/* reset buffer */
	bufCur = buf;
	bufEnd = bufCur;
}

size_t GPS_PARSER_getFrame(uint8_t * buf, size_t bufLen)
{
	assert (bufLen > 0);
	while (true) {
		/* synchronize */
		while (true) {
			uint8_t sync;
			if (unlikely(!next(&sync)))
				return false;
			if (sync == 0x10)
				break;
			NOC_fprintf(stderr, "GPS: Out of Sync: got 0x%2d instead of "
				"0x10\n", sync);
			//++STAT_stats.ADSB_outOfSync; TODO
			if (!synchronize())
				return false;
		}

		/* decode frame */
decode_frame:
		/* decode type */
		if (unlikely(!next(buf)))
			return false;
		if (unlikely(*buf == 0x03)) {
			/* TODO: resync */
			continue;
		}
		size_t len = bufLen;
		enum DECODE_STATUS rs = decode(buf, &len);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			//++STAT_stats.ADSB_outOfSync; TODO
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL)) {
			return 0;
		} else if (unlikely(rs == DECODE_STATUS_BUFFER_FULL)) {
			/* TODO */
			continue;
		}
		return len;
	}
}

static inline enum DECODE_STATUS decode(uint8_t * dst, size_t * len)
{
	size_t dstLen = *len;
	uint8_t * const dstStart = dst;

	do {
		if (unlikely(bufCur == bufEnd)) {
			/* current buffer is empty: fill it again */
			if (unlikely(!discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
		}
		/* search escape in the current buffer end up to the expected frame
		 * length */
		size_t rbuf = bufEnd - bufCur;
		size_t mlen = rbuf <= dstLen ? rbuf : dstLen;
		uint8_t * esc = memchr(bufCur, '\x10', mlen);
		if (likely(esc)) {
			/* escape found: copy buffer up to escape, if applicable */
			if (likely(esc != bufCur)) {
				memcpy(dst, bufCur, esc - bufCur);
				dst += esc - bufCur;
				dstLen -= esc - bufCur;
			}

			/* discard escape */
			bufCur = esc + 1;

			/* Peek the next symbol. */
			if (unlikely(bufCur == bufEnd && !discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
			if (likely(*bufCur == '\x03')) {
				/* frame end: return */
				*len = dst - dstStart;
				return DECODE_STATUS_OK;
			}
			if (likely(*bufCur == '\x10')) {
				/* it's another escape -> append escape */
				*dst++ = '\x10';
				--dstLen;
				++bufCur;
			} else {
				/* synchronization failure */
				return DECODE_STATUS_RESYNC;
			}
		} else {
			/* no escape found: copy up to buffer length */
			memcpy(dst, bufCur, mlen);
			dst += mlen;
			bufCur += mlen;
			dstLen -= mlen;
		}
		/* loop as there are symbols left */
	} while (likely(dstLen));
	return DECODE_STATUS_BUFFER_FULL;
}

/** Discard buffer content and fill it again. */
static inline bool discardAndFill()
{
	size_t rc = GPS_INPUT_read(buf, sizeof buf);
	if (unlikely(rc == 0)) {
		return false;
	} else {
		bufCur = buf;
		bufEnd = buf + rc;
		return true;
	}
}

/** Consume next character from buffer.
 * \return next character in buffer
 */
static inline bool next(uint8_t * ch)
{
	do {
		if (likely(bufCur < bufEnd)) {
			*ch = *bufCur++;
			return true;
		}
	} while (likely(discardAndFill()));
	return false;
}

/** Synchronize buffer.
 * \note After calling that, the next call to next() or peek() will return 0x10.
 */
static inline bool synchronize()
{
	do {
		uint8_t * esc = memchr(bufCur, 0x10, bufEnd - bufCur);
		if (likely(esc)) {
			bufCur = esc;
			return true;
		}
	} while (likely(discardAndFill()));
	return false;
}
