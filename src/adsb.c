/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _DEFAULT_SOURCE
#include <log.h>
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

static const char PFX[] = "ADSB";

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
	struct ADSB_RawFrame * raw);

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
#ifndef INPUT_LAYER_NETWORK
	const struct CFG_RECV * cfg = &CFG_config.recv;
	/* setup ADSB */
	return setOption(ADSB_OPTION_OUTPUT_FORMAT_BIN) &&
		setOption(cfg->modeSLongExtSquitter ?
			ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
			ADSB_OPTION_FRAME_FILTER_ALL) &&
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
#else
	return true;
#endif
}

void ADSB_reconfigure()
{
#ifndef INPUT_LAYER_NETWORK
	setOption(CFG_config.recv.modeSLongExtSquitter ?
		ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
		ADSB_OPTION_FRAME_FILTER_ALL);
#endif
}

/** Set an option for the ADSB decoder.
 * \param option option to be set
 */
__attribute__((unused))
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

bool ADSB_getFrame(struct ADSB_RawFrame * raw,
	struct ADSB_DecodedFrame * decoded)
{
	raw->raw[0] = 0x1a;

	while (true) {
		/* synchronize */
		uint8_t sync;
		if (unlikely(!next(&sync)))
			return false;
		if (unlikely(sync != 0x1a)) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Out of Sync: got 0x%02" PRIx8
				" instead of 0x1a", sync);
			++STAT_stats.RECV_outOfSync;
synchronize:
			if (unlikely(!synchronize()))
				return false;
		}

		/* decode type */
		uint8_t type;
decode_frame:
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
			LOG_log(LOG_LEVEL_WARN, PFX, "Out of Sync: got unescaped 0x1a in "
				"frame, resynchronizing");
			++STAT_stats.RECV_outOfSync;
			goto synchronize;
		default:
			LOG_logf(LOG_LEVEL_WARN, PFX, "Unknown frame type %c, "
				"resynchronizing", type);
			++STAT_stats.RECV_frameTypeUnknown;
			goto synchronize;
		}
		decoded->frameType = type - '1';
		decoded->payloadLen = payload_len;
		raw->raw[1] = type;
		raw->raw_len = 2;

		/* decode header */
		__attribute__((aligned(8))) union { uint8_t u8[7]; uint64_t u64; } hdr;
		enum DECODE_STATUS rs = decode(hdr.u8, sizeof hdr.u8, raw);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++STAT_stats.RECV_outOfSync;
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;
		/* decode mlat and signal level */
		decoded->mlat = be64toh(hdr.u64) >> 16;
		decoded->siglevel = hdr.u8[6];

		/* read payload */
		rs = decode(decoded->payload, payload_len, raw);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++STAT_stats.RECV_outOfSync;
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
	struct ADSB_RawFrame * raw)
{
	/* get current raw ptr */
	uint8_t * rawPtr = raw->raw + raw->raw_len;

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
			raw->raw_len += esc + 1 - bufCur;

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
				++raw->raw_len;
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
			raw->raw_len += mlen;
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
 * \note Furthermore, *bufCur != 0x1a, because a frame cannot start with \x1a
 */
static inline bool synchronize()
{
	do {
		uint8_t * sync;
redo:
		sync = memchr(bufCur, 0x1a, bufEnd - bufCur);
		if (likely(sync)) {
			/* found sync symbol, discard everything including the sync */
			bufCur = sync + 1;

			/* peek next */
			if (unlikely(bufCur == bufEnd)) {
				/* we have to receive more data to peek */
				if (unlikely(!discardAndFill()))
					return false;
			}

			if (unlikely(*bufCur == 0x1a)) {
				/* next symbol is an escaped sync
				 * -> discard it and try again */
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
