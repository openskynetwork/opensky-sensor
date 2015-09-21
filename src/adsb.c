/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _DEFAULT_SOURCE
#include <error.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <endian.h>
#include <stdio.h>
#include <inttypes.h>
#include <adsb.h>
#include <statistics.h>
#include <unistd.h>
#include <input.h>
#include <cfgfile.h>
#include <threads.h>
#include <util.h>

/** receive buffer */
static uint8_t buf[128];
/** current pointer into buffer */
static uint8_t * bufCur;
/** buffer end */
static uint8_t * bufEnd;

/** ADSB Options */
enum ADSB_OPTION {
	/** Output Format: AVR */
	ADSB_OPTION_OUTPUT_FORMAT_AVR = 'c',
	/** Output Format: Binary */
	ADSB_OPTION_OUTPUT_FORMAT_BIN = 'C',

	/** Filter: output DF-11/17/18 frames only */
	ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY = 'D',
	/** Filter: output all frames */
	ADSB_OPTION_FRAME_FILTER_ALL = 'd',

	/** MLAT Information: include MLAT when using AVR Output format */
	ADSB_OPTION_AVR_FORMAT_MLAT = 'E',
	/** MLAT Information: don't include MLAT when using AVR Output format */
	ADSB_OPTION_AVR_FORMAT_NO_MLAT = 'e',

	/** CRC: check DF-11/17/18 frames */
	ADSB_OPTION_DF_11_17_18_CRC_ENABLED = 'f',
	/** CRC: don't check DF-11/17/18 frames */
	ADSB_OPTION_DF_11_17_18_CRC_DISABLED = 'F',

	/** Timestamp Source: GPS */
	ADSB_OPTION_TIMESTAMP_SOURCE_GPS = 'G',
	/** Timestamp Source: Legacy 12 MHz Clock */
	ADSB_OPTION_TIMESTAMP_SOURCE_LEGACY_12_MHZ = 'g',

	/** RTS Handshake: enabled */
	ADSB_OPTION_RTS_HANDSHAKE_ENABLED = 'H',
	/** RTS Handshake: disabled */
	ADSB_OPTION_RTS_HANDSHAKE_DISABLED = 'h',

	/** FEC: enable error correction on DF-11/17/18 frames */
	ADSB_OPTION_DF_17_18_FEC_ENABLED = 'i',
	/** FEC: disable error correction on DF-11/17/18 frames */
	ADSB_OPTION_DF_17_18_FEC_DISABLED = 'I',

	/** Mode-AC messages: enable decoding */
	ADSB_OPTION_MODE_AC_DECODING_ENABLED = 'J',
	/** Mode-AC messages: disable decoding */
	ADSB_OPTION_MODE_AC_DECODING_DISABLED = 'j'
};

enum DECODE_STATUS {
	DECODE_STATUS_OK,
	DECODE_STATUS_RESYNC,
	DECODE_STATUS_CONNFAIL
};

static bool configure();
static inline bool discardAndFill();
static inline bool next(uint8_t * ch);
static inline bool synchronize();
static inline bool setOption(enum ADSB_OPTION option);
static inline enum DECODE_STATUS decode(uint8_t * buf, size_t len,
	struct ADSB_Frame * frame);

void ADSB_init()
{
	INPUT_init();
}

void ADSB_destruct()
{
	INPUT_destruct();
}

/** Setup ADSB receiver with some options. */
static bool configure()
{
#ifndef NETWORK
	const struct CFG_RECV * cfg = &CFG_config.recv;
	/* setup ADSB */
	return setOption(ADSB_OPTION_OUTPUT_FORMAT_BIN) &&
		setOption(ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY) &&
		setOption(ADSB_OPTION_AVR_FORMAT_MLAT) &&
		setOption(cfg->crc ? ADSB_OPTION_DF_11_17_18_CRC_ENABLED :
			ADSB_OPTION_DF_11_17_18_CRC_DISABLED) &&
		setOption(cfg->gps ? ADSB_OPTION_TIMESTAMP_SOURCE_GPS :
			ADSB_OPTION_TIMESTAMP_SOURCE_LEGACY_12_MHZ) &&
		setOption(CFG_config.input.rtscts ? ADSB_OPTION_RTS_HANDSHAKE_ENABLED :
			ADSB_OPTION_RTS_HANDSHAKE_DISABLED) &&
		setOption(cfg->fec ? ADSB_OPTION_DF_17_18_FEC_ENABLED :
			ADSB_OPTION_DF_17_18_FEC_DISABLED) &&
		setOption(ADSB_OPTION_MODE_AC_DECODING_DISABLED);
#endif
}

/** Set an option for the ADSB decoder.
 * \param option option to be set
 */
static inline bool setOption(enum ADSB_OPTION option)
{
	uint8_t w[3] = { '\x1a', '1', (uint8_t)option };
	return INPUT_write(w, sizeof w) == sizeof w;
}

/** ADSB */
void ADSB_connect()
{
	while (true) {
		/* connect with input */
		INPUT_connect();

		/* configure input */
		if (configure())
			break;
	}

	/* reset buffer */
	bufCur = buf;
	bufEnd = bufCur;
}

bool ADSB_getFrame(struct ADSB_Frame * frame)
{
	while (true) {
		/* synchronize */
		while (true) {
			uint8_t sync;
			if (unlikely(!next(&sync)))
				return false;
			if (sync == 0x1a)
				break;
			NOC_fprintf(stderr, "ADSB: Out of Sync: got 0x%2d instead of "
				"0x1a\n", sync);
			++STAT_stats.ADSB_outOfSync;
			if (!synchronize())
				return false;
		}

		/* decode frame */
		frame->raw[0] = 0x1a;
		uint8_t type;
decode_frame:
		/* decode type */
		if (unlikely(!next(&type)))
			return false;
		size_t payload_len;
		switch (type) {
		case '1': /* mode-ac */
			payload_len = 2;
		break;
		case '2': /* mode-s short */
			payload_len = 7;
		break;
		case '3': /* mode-s long */
			payload_len = 14;
		break;
		case '4': /* status frame */
			payload_len = 14;
		break;
		case '\x1a': /* resynchronize */
			++STAT_stats.ADSB_outOfSync;
			NOC_puts("ADSB: Out of Sync: got unescaped 0x1a in frame, "
				"treating as resynchronization");
			goto decode_frame;
		default:
			NOC_fprintf(stderr, "ADSB: Unknown frame type %c, "
				"resynchronizing\n", type);
			++STAT_stats.ADSB_frameTypeUnknown;
			if (!synchronize())
				return false;
			continue;
		}
		frame->frameType = type - '1';
		frame->payloadLen = payload_len;
		frame->raw[1] = type;
		frame->raw_len = 2;

		/* read header */
		__attribute__((aligned(8))) uint8_t header[8];
		enum DECODE_STATUS rs = decode(header, 7, frame);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++STAT_stats.ADSB_outOfSync;
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;
		/* decode mlat and signal level */
		frame->mlat = be64toh(*(uint64_t*)header) >> 16;
		frame->siglevel = header[6];

		/* read payload */
		rs = decode(frame->payload, payload_len, frame);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++STAT_stats.ADSB_outOfSync;
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;

		return true;
	}
}

/** Unescape frame payload.
 * \param dst destination buffer
 * \param len length of frame content to be decoded.
 *  Must not exceed the buffers size
 * \param frame frame to be decoded, used for raw decoding
 */
static inline enum DECODE_STATUS decode(uint8_t * dst, size_t len,
	struct ADSB_Frame * frame)
{
	size_t i;
	/* get current raw ptr */
	uint8_t * rawPtr = frame->raw + frame->raw_len;

	do {
		if (unlikely(bufCur == bufEnd)) {
			/* current buffer is empty: fill it again */
			if (unlikely(!discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
		}
		/* search escape in the current buffer end up to the expected frame
		 * length */
		size_t rbuf = bufEnd - bufCur;
		size_t mlen = rbuf <= len ? rbuf : len;
		uint8_t * esc = memchr(bufCur, '\x1a', mlen);
		if (unlikely(esc)) {
			/* escape found: copy buffer up to escape, if applicable */
			memcpy(rawPtr, bufCur, esc + 1 - bufCur);
			rawPtr += esc + 1 - bufCur;
			frame->raw_len += esc + 1 - bufCur;

			if (likely(esc != bufCur)) {
				memcpy(dst, bufCur, esc - bufCur);
				dst += esc - bufCur;
				len -= esc - bufCur;
			}

			/* discard escape */
			bufCur = esc + 1;

			/* Peek the next symbol. */
			if (unlikely(bufCur == bufEnd && !discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
			if (likely(*bufCur == '\x1a')) {
				/* it's another escape -> append escape */
				*dst++ = '\x1a';
				*rawPtr++ = '\x1a';
				++frame->raw_len;
				--len;
				++bufCur;
			} else {
				/* synchronization failure */
				return DECODE_STATUS_RESYNC;
			}
		} else {
			/* no escape found: copy up to buffer length */
			memcpy(dst, bufCur, mlen);
			memcpy(rawPtr, bufCur, mlen);
			rawPtr += mlen;
			frame->raw_len += mlen;
			dst += mlen;
			bufCur += mlen;
			len -= mlen;
		}
		/* loop as there are symbols left */
	} while (unlikely(len));
	return DECODE_STATUS_OK;
}

/** Discard buffer content and fill it again. */
static inline bool discardAndFill()
{
	size_t rc = INPUT_read(buf, sizeof buf);
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
 * \note After calling that, the next call to next() or peek() will return 0x1a.
 */
static inline bool synchronize()
{
	do {
		uint8_t * esc = memchr(bufCur, 0x1a, bufEnd - bufCur);
		if (likely(esc)) {
			bufCur = esc;
			return true;
		}
	} while (likely(discardAndFill()));
	return false;
}
