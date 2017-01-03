/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "trimble_parser.h"
#include "trimble_input.h"
#include "util/log.h"
#include "util/util.h"
#include "util/threads.h"

/** Component: Prefix */
static const char PFX[] = "TRIMBLE";

#define BOM 0x10
#define EOM 0x03

/** Decoding status */
enum DECODE_STATUS {
	/** Success */
	DECODE_STATUS_OK,
	/** Unexpected resynchronization */
	DECODE_STATUS_RESYNC,
	/** Connection failure */
	DECODE_STATUS_CONNFAIL,
	/** Buffer full */
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

/** Initialize trimble parser */
void TRIMBLE_PARSER_init()
{
	TRIMBLE_INPUT_init();
}

/** Setup trimble receiver with some options.
 * @return true if writing to the receiver was successful
 */
static bool configure()
{
	const uint8_t setutc[] = { BOM, 0x35, 0x04, 0x00, 0x01, 0x08, BOM, EOM };
	return TRIMBLE_INPUT_write(setutc, sizeof setutc) == sizeof setutc;
}

/** Connect to receiver and configure it */
void TRIMBLE_PARSER_connect()
{
	while (true) {
		/* connect with input */
		TRIMBLE_INPUT_connect();

		/* configure input */
		if (configure())
			break;
	}

	/* reset buffer */
	bufCur = buf;
	bufEnd = bufCur;
}

/** Disconnect from receiver */
void TRIMBLE_PARSER_disconnect()
{
	TRIMBLE_INPUT_disconnect();
}

/** Get a frame from the receiver
 * @param buf buffer to receive into
 * @param bufLen size of buffer to receive into
 * @return length which was actually used
 */
size_t TRIMBLE_PARSER_getFrame(uint8_t * buf, size_t bufLen)
{
	assert (bufLen > 0);

	while (true) {
		/* synchronize */
		uint8_t bom;
		if (unlikely(!next(&bom)))
			return false;
		if (unlikely(bom != BOM)) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Out of Sync: got 0x%02" PRIx8
				" instead of BOM", bom);
synchronize:
			if (unlikely(!synchronize()))
				return false;
		}

		/* decode frame */
decode_frame:
		/* decode type */
		if (unlikely(!next(buf)))
			return false;
		if (unlikely(*buf == EOM)) {
			LOG_log(LOG_LEVEL_WARN, PFX, "Out of Sync: got unescaped EOM in "
				"frame, resynchronizing");
			goto synchronize;
		}
		size_t len = bufLen - 1;
		enum DECODE_STATUS rs = decode(buf + 1, &len);
		if (likely(rs == DECODE_STATUS_OK)) {
			return len + 1;
		} else {
			switch (rs) {
			case DECODE_STATUS_RESYNC:
				goto decode_frame;
			case DECODE_STATUS_CONNFAIL:
				return 0;
			case DECODE_STATUS_BUFFER_FULL:
				goto synchronize;
			default:
				assert(false);
			}
		}
	}
}

/** Decode a trimble frame
 * @param dst destination buffer
 * @param len pointer to size of buffer, will contain the actually used length
 *  after return
 * @return decoding status
 */
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
		/* search BOM in the current buffer end up to remaining destination
		 * buffer length */
		size_t rbuf = bufEnd - bufCur;
		size_t mlen = rbuf <= dstLen ? rbuf : dstLen;
		uint8_t * bom = memchr(bufCur, BOM, mlen);
		if (likely(bom)) {
			/* BOM found: copy buffer up to BOM, if applicable */
			size_t syncLen = bom - bufCur;
			if (likely(syncLen)) {
				memcpy(dst, bufCur, syncLen);
				dst += syncLen;
				dstLen -= syncLen;
			}

			/* consume symbols, discard BOM */
			bufCur = bom + 1;

			/* peek next symbol */
			if (unlikely(bufCur == bufEnd && !discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
			if (likely(*bufCur == EOM)) {
				/* frame end: discard symbol and return */
				++bufCur;
				*len = dst - dstStart;
				return DECODE_STATUS_OK;
			}
			if (likely(*bufCur == BOM)) {
				/* it's another BOM -> append escaped BOM */
				if (unlikely(!dstLen))
					break;
				*dst++ = BOM;
				--dstLen;
				++bufCur;
			} else {
				/* synchronization failure */
				return DECODE_STATUS_RESYNC;
			}
		} else {
			/* no BOM found: copy up to buffer length */
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

/** Discard buffer content and fill it again.
 * @return true if reading was successful
 */
static inline bool discardAndFill()
{
	size_t rc = TRIMBLE_INPUT_read(buf, sizeof buf);
	if (unlikely(rc == 0)) {
		return false;
	} else {
		bufCur = buf;
		bufEnd = buf + rc;
		return true;
	}
}

/** Consume next symbol from buffer.
 * @param ch pointer to returned next symbol
 * @return true if reading new data was successful
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
 * @return true if reading new data was successful
 * @note After calling that, *bufCur will be the first byte of the frame
 * @note It is also guaranteed, that bufCur != bufEnd
 * @note Furthermore, *bufCur != BOM and *bufCur != EOM, because a frame
 *  cannot start with BOM or EOM
 */
static inline bool synchronize()
{
	do {
		uint8_t * bom;
redo:
		bom = memchr(bufCur, BOM, bufEnd - bufCur);
		if (likely(bom)) {
			/* found BOM, discard everything including the BOM */
			bufCur = bom + 1;

			/* peek next */
			if (unlikely(bufCur == bufEnd)) {
				/* we have to receive more data to peek */
				if (unlikely(!discardAndFill()))
					return false;
			}

			if (unlikely(*bufCur == BOM || *bufCur == EOM)) {
				/* next symbol is an escaped BOM or an EOM
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
