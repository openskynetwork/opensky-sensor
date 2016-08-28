/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "gps_parser.h"
#include "gps_input.h"
#include "util.h"
#include "threads.h"

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
		uint8_t sync;
		if (unlikely(!next(&sync)))
			return false;
		if (unlikely(sync != 0x10)) {
			NOC_fprintf(stderr, "GPS: Out of Sync: got 0x%2" PRIx8
				" instead of 0x10\n", sync);
			//++STAT_stats.ADSB_outOfSync;
synchronize:
			if (unlikely(!synchronize()))
				return false;
		}

		/* decode frame */
decode_frame:
		/* decode type */
		if (unlikely(!next(buf)))
			return false;
		if (unlikely(*buf == 0x03)) {
			NOC_puts("GPS: Out of Sync: got unescaped 0x03 in frame, "
				"resynchronizing");
			//++STAT_stats.ADSB_outOfSync;
			goto synchronize;
		}
		size_t len = bufLen - 1;
		enum DECODE_STATUS rs = decode(buf + 1, &len);
		if (likely(rs == DECODE_STATUS_OK)) {
			return len + 1;
		} else {
			switch (rs) {
			case DECODE_STATUS_RESYNC:
				//++STAT_stats.ADSB_outOfSync; TODO
				goto decode_frame;
			case DECODE_STATUS_CONNFAIL:
				return 0;
			case DECODE_STATUS_BUFFER_FULL:
				/* TODO: stats */
				goto synchronize;
			default:
				assert(false);
			}
		}
	}
}

static inline enum DECODE_STATUS decode(uint8_t * dst, size_t * len)
{
	/* destination buffer length */
	size_t dstLen = *len;
	/* start of destination buffer */
	uint8_t * const dstStart = dst;

	do {
		if (unlikely(bufCur == bufEnd)) {
			/* buffer is empty: fill it again */
			if (unlikely(!discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
		}
		/* search sync in the current buffer end up to remaining destination
		 * buffer length */
		size_t rbuf = bufEnd - bufCur;
		size_t mlen = rbuf <= dstLen ? rbuf : dstLen;
		uint8_t * sync = memchr(bufCur, '\x10', mlen);
		if (likely(sync)) {
			/* sync found: copy buffer up to escape, if applicable */
			size_t syncLen = sync - bufCur;
			if (likely(syncLen)) {
				memcpy(dst, bufCur, syncLen);
				dst += syncLen;
				dstLen -= syncLen;
			}

			/* consume symbols, discard sync */
			bufCur = sync + 1;

			/* peek next symbol */
			if (unlikely(bufCur == bufEnd && !discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
			if (likely(*bufCur == '\x03')) {
				/* frame end: discard symbol and return */
				++bufCur;
				*len = dst - dstStart;
				return DECODE_STATUS_OK;
			}
			if (likely(*bufCur == '\x10')) {
				/* it's another sync -> append escape */
				if (unlikely(!dstLen))
					break;
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
		/* loop as there is still some space in the destination buffer */
	} while (likely(dstLen));
	/* destination buffer is full */
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

/** Consume next symbol from buffer.
 * \param ch pointer to returned next symbol
 * \return false if reading new data failed, true otherwise
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
 * \return false if reading new data failed, true otherwise
 * \note After calling that, *bufCur will be the first byte of the frame
 * \note It is also guaranteed, that bufCur != bufEnd
 * \note Furthermore, *bufCur != 0x1a and *bufCur != 0x03, because a frame
 *  cannot start with \x1a or \x03
 */
static inline bool synchronize()
{
	do {
		uint8_t * sync;
redo:
		sync = memchr(bufCur, 0x10, bufEnd - bufCur);
		if (likely(sync)) {
			/* found sync symbol, discard everything including the sync */
			bufCur = sync + 1;

			/* peek next */
			if (unlikely(bufCur == bufEnd)) {
				/* we have to receive more data to peek */
				if (unlikely(!discardAndFill()))
					return false;
			}

			if (unlikely(*bufCur == 0x10 || *bufCur == 0x03)) {
				/* next symbol is an escaped sync or an end-of-frame symbol
				 * -> discard both and try again */
				++bufCur;
				if (bufCur != bufEnd)
					goto redo;
			} else {
				return true;
			}
		}
	} while (likely(discardAndFill()));
	return false;
}
