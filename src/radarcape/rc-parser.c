/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _DEFAULT_SOURCE
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <unistd.h>
#include "rc-parser.h"
#include "rc-input.h"
#include "core/openskytypes.h"
#include "core/filter.h"
#include "core/beast.h"
#include "util/statistics.h"
#include "util/log.h"
#include "util/cfgfile.h"
#include "util/threads.h"
#include "util/util.h"
#include "util/port/endian.h"

_Static_assert((int)BEAST_TYPE_MODE_AC == (int)OPENSKY_FRAME_TYPE_MODE_AC &&
	(int)BEAST_TYPE_MODE_S_SHORT == (int)OPENSKY_FRAME_TYPE_MODE_S_SHORT &&
	(int)BEAST_TYPE_MODE_S_LONG == (int)OPENSKY_FRAME_TYPE_MODE_S_LONG &&
	(int)BEAST_TYPE_STATUS == (int)OPENSKY_FRAME_TYPE_STATUS,
	"Message type mismatch");

/** Component: Prefix */
static const char PFX[] = "RC";

/** receive buffer */
static uint8_t buf[128];
/** current pointer into buffer */
static uint8_t * bufCur;
/** buffer end */
static uint8_t * bufEnd;

static struct RC_PARSER_Statistics stats;

/** Radarcape options */
enum RADARCAPE_OPTION {
	/** Output Format: AVR */
	RADARCAPE_OPTION_OUTPUT_FORMAT_AVR = 'c',
	/** Output Format: Binary */
	RADARCAPE_OPTION_OUTPUT_FORMAT_BIN = 'C',

	/** Filter: output DF-11/17/18 frames only */
	RADARCAPE_OPTION_FRAME_FILTER_DF_11_17_18_ONLY = 'D',
	/** Filter: output all frames */
	RADARCAPE_OPTION_FRAME_FILTER_ALL = 'd',

	/** MLAT Information: include MLAT when using AVR Output format */
	RADARCAPE_OPTION_AVR_FORMAT_MLAT = 'E',
	/** MLAT Information: don't include MLAT when using AVR Output format */
	RADARCAPE_OPTION_AVR_FORMAT_NO_MLAT = 'e',

	/** CRC: check DF-11/17/18 frames */
	RADARCAPE_OPTION_DF_11_17_18_CRC_ENABLED = 'f',
	/** CRC: don't check DF-11/17/18 frames */
	RADARCAPE_OPTION_DF_11_17_18_CRC_DISABLED = 'F',

	/** Timestamp Source: GPS */
	RADARCAPE_OPTION_TIMESTAMP_SOURCE_GPS = 'G',
	/** Timestamp Source: Legacy 12 MHz Clock */
	RADARCAPE_OPTION_TIMESTAMP_SOURCE_LEGACY_12_MHZ = 'g',

	/** RTS Handshake: enabled */
	RADARCAPE_OPTION_RTS_HANDSHAKE_ENABLED = 'H',
	/** RTS Handshake: disabled */
	RADARCAPE_OPTION_RTS_HANDSHAKE_DISABLED = 'h',

	/** FEC: enable error correction on DF-11/17/18 frames */
	RADARCAPE_OPTION_DF_17_18_FEC_ENABLED = 'i',
	/** FEC: disable error correction on DF-11/17/18 frames */
	RADARCAPE_OPTION_DF_17_18_FEC_DISABLED = 'I',

	/** Mode-AC messages: enable decoding */
	RADARCAPE_OPTION_MODE_AC_DECODING_ENABLED = 'J',
	/** Mode-AC messages: disable decoding */
	RADARCAPE_OPTION_MODE_AC_DECODING_DISABLED = 'j',

	/** Radarcape LEDs: yellow */
	RADARCAPE_OPTION_Y = 'Y',
	/** Radarcape LEDs: red */
	RADARCAPE_OPTION_R = 'R'
};

/** Decoding status */
enum DECODE_STATUS {
	/** Success */
	DECODE_STATUS_OK,
	/** Unexpected resynchronization */
	DECODE_STATUS_RESYNC,
	/** Connection failure */
	DECODE_STATUS_CONNFAIL
};

/** Configuration: Forward error correction */
static bool cfgFec;

/** Configuration Descriptor */
static struct CFG_Section cfgDesc = {
	.name = "INPUT",
	.n_opt = 1,
	.options = {
		{
			.name = "fec",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfgFec,
			.def = {
				.boolean = true
			}
		}
	}
};

static bool construct();
static bool configure();
static inline bool discardAndFill();
static inline bool next(uint8_t * ch);
static inline bool synchronize();
static inline bool setOption(enum RADARCAPE_OPTION option);
static inline enum DECODE_STATUS decode(uint8_t * buf, size_t len,
	struct OPENSKY_RawFrame * raw);
static void resetStats();

/** Component Descriptor */
const struct Component INPUT_comp = {
	.description = PFX,
	.onRegister = &RC_INPUT_register,
	.onConstruct = &construct,
	.config = &cfgDesc,
	.onReset = &resetStats,
	.dependencies = { NULL }
};

/** Initialize parser
 * @return always true
 */
static bool construct()
{
	RC_INPUT_init();
	return true;
}

/** Setup ADSB receiver with some options.
 * @return true if configuration could be set successfully
 */
static bool configure()
{
#if defined(INPUT_RADARCAPE_UART) || defined(CHECK)
	const struct FILTER_Configuration * filterCfg = FILTER_getConfiguration();
	/* setup ADSB */
	return setOption(RADARCAPE_OPTION_OUTPUT_FORMAT_BIN) &&
		setOption(filterCfg->extSquitter ?
			RADARCAPE_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
			RADARCAPE_OPTION_FRAME_FILTER_ALL) &&
		setOption(RADARCAPE_OPTION_AVR_FORMAT_MLAT) &&
		setOption(filterCfg->crc ?
			RADARCAPE_OPTION_DF_11_17_18_CRC_ENABLED :
			RADARCAPE_OPTION_DF_11_17_18_CRC_DISABLED) &&
		setOption(RADARCAPE_OPTION_TIMESTAMP_SOURCE_GPS) &&
		setOption(RADARCAPE_OPTION_RTS_HANDSHAKE_ENABLED) &&
		setOption(cfgFec ? RADARCAPE_OPTION_DF_17_18_FEC_ENABLED :
			RADARCAPE_OPTION_DF_17_18_FEC_DISABLED) &&
		setOption(RADARCAPE_OPTION_MODE_AC_DECODING_DISABLED) &&
		setOption(RADARCAPE_OPTION_Y) &&
		setOption(RADARCAPE_OPTION_R);
#else
	return true;
#endif
}

/** Reconfigure the input from the filter configuration */
void INPUT_reconfigure()
{
#ifdef INPUT_RADARCAPE_UART
	const struct FILTER_Configuration * filterCfg = FILTER_getConfiguration();
	setOption(filterCfg->extSquitter ?
		RADARCAPE_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
		RADARCAPE_OPTION_FRAME_FILTER_ALL);
#endif
}

/** Set an option for the ADSB decoder.
 * @param option option to be set
 */
__attribute__((unused))
static inline bool setOption(enum RADARCAPE_OPTION option)
{
	uint8_t w[3] = { BEAST_SYNC, '1', (uint8_t)option };
	return RC_INPUT_write(w, sizeof w) == sizeof w;
}

/** Connect input */
void INPUT_connect()
{
	while (true) {
		/* connect with input */
		RC_INPUT_connect();

		/* configure input */
		if (configure())
			break;
	}

	/* reset buffer */
	bufCur = buf;
	bufEnd = bufCur;
}

/** Disconnect input */
void INPUT_disconnect()
{
	RC_INPUT_disconnect();
}

/** Get a frame from the receiver
 * @param raw raw frame to receive into
 * @param decoded decoded frame to receive into
 * @return true on success, false on connection failure
 */
bool INPUT_getFrame(struct OPENSKY_RawFrame * raw,
	struct OPENSKY_DecodedFrame * decoded)
{
	raw->raw[0] = BEAST_SYNC;

	while (true) {
		/* synchronize */
		uint8_t sync;
		if (unlikely(!next(&sync)))
			return false;
		if (unlikely(sync != BEAST_SYNC)) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Out of Sync: got 0x%02" PRIx8
				" instead of 0x%02x", sync, BEAST_SYNC);
			++stats.outOfSync;
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
		case BEAST_TYPE_MODE_AC: /* mode-ac */
			payload_len = 2;
		break;
		case BEAST_TYPE_MODE_S_SHORT: /* mode-s short */
			payload_len = 7;
		break;
		case BEAST_TYPE_MODE_S_LONG: /* mode-s long */
			payload_len = 14;
		break;
		case BEAST_TYPE_STATUS: /* status frame */
			payload_len = 14;
		break;
		case BEAST_SYNC: /* resynchronize */
			LOG_log(LOG_LEVEL_WARN, PFX, "Out of Sync: got unescaped SYNC "
				"in frame, resynchronizing");
			++stats.outOfSync;
			goto synchronize;
		default:
			LOG_logf(LOG_LEVEL_WARN, PFX, "Unknown frame type %c, "
				"resynchronizing", type);
			++stats.frameTypeUnknown;
			goto synchronize;
		}
		decoded->frameType = type;
		decoded->payloadLen = payload_len;
		raw->raw[1] = type;
		raw->rawLen = 2;

		/* decode header */
		__attribute__((aligned(8))) union { uint8_t u8[7]; uint64_t u64; } hdr;
		enum DECODE_STATUS rs = decode(hdr.u8, sizeof hdr.u8, raw);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++stats.outOfSync;
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;
		/* decode mlat and signal level */
		decoded->mlat = be64toh(hdr.u64) >> 16;
		decoded->siglevel = hdr.u8[6];

		/* read payload */
		rs = decode(decoded->payload, payload_len, raw);
		if (unlikely(rs == DECODE_STATUS_RESYNC)) {
			++stats.outOfSync;
			goto decode_frame;
		} else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;

		return true;
	}
}

/** Unescape frame payload.
 * @param dst destination buffer
 * @param len length of frame content to be decoded.
 *  Must not exceed the buffers size
 * @param raw raw frame to copy the raw data into
 */
static inline enum DECODE_STATUS decode(uint8_t * dst, size_t len,
	struct OPENSKY_RawFrame * raw)
{
	/* get current raw ptr */
	uint8_t * rawPtr = raw->raw + raw->rawLen;

	do {
		if (unlikely(bufCur == bufEnd)) {
			/* current buffer is empty: fill it again */
			if (unlikely(!discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
		}
		/* search SYNC in the current buffer end up to the expected frame
		 * length */
		size_t rbuf = bufEnd - bufCur;
		size_t mlen = rbuf <= len ? rbuf : len;
		uint8_t * sync = memchr(bufCur, BEAST_SYNC, mlen);
		if (unlikely(sync)) {
			/* SYNC found: copy buffer up to escape, if applicable */
			memcpy(rawPtr, bufCur, sync + 1 - bufCur);
			rawPtr += sync + 1 - bufCur;
			raw->rawLen += sync + 1 - bufCur;

			if (likely(sync != bufCur)) {
				memcpy(dst, bufCur, sync - bufCur);
				dst += sync - bufCur;
				len -= sync - bufCur;
			}

			/* discard SYNC */
			bufCur = sync + 1;

			/* Peek the next symbol. */
			if (unlikely(bufCur == bufEnd && !discardAndFill()))
				return DECODE_STATUS_CONNFAIL;
			if (likely(*bufCur == BEAST_SYNC)) {
				/* it's another SYNC -> append escaped SYNC */
				*dst++ = BEAST_SYNC;
				*rawPtr++ = BEAST_SYNC;
				++raw->rawLen;
				--len;
				++bufCur;
			} else {
				/* synchronization failure */
				return DECODE_STATUS_RESYNC;
			}
		} else {
			/* no SYNC found: copy up to buffer length */
			memcpy(dst, bufCur, mlen);
			memcpy(rawPtr, bufCur, mlen);
			rawPtr += mlen;
			raw->rawLen += mlen;
			dst += mlen;
			bufCur += mlen;
			len -= mlen;
		}
		/* loop as there are symbols left */
	} while (unlikely(len));
	return DECODE_STATUS_OK;
}

/** Discard buffer content and fill it again.
 * @return true if reading was successful
 */
static inline bool discardAndFill()
{
	size_t rc = RC_INPUT_read(buf, sizeof buf);
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
 * @note Furthermore, *bufCur != BEAST_SYNC, because a frame cannot start
 *  with BEAST_SYNC
 */
static inline bool synchronize()
{
	do {
		uint8_t * sync;
redo:
		sync = memchr(bufCur, BEAST_SYNC, bufEnd - bufCur);
		if (likely(sync)) {
			/* found SYNC symbol, discard everything including the SYNC */
			bufCur = sync + 1;

			/* peek next */
			if (unlikely(bufCur == bufEnd)) {
				/* we have to receive more data to peek */
				if (unlikely(!discardAndFill()))
					return false;
			}

			if (unlikely(*bufCur == BEAST_SYNC)) {
				/* next symbol is an escaped SYNC -> discard it and try again */
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

static void resetStats()
{
	memset(&stats, 0, sizeof stats);
}

void RC_PARSER_getStatistics(struct RC_PARSER_Statistics * statistics)
{
	memcpy(statistics, &stats, sizeof *statistics);
}
