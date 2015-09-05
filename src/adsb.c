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
/** current buffer length (max sizeof buf) */
static size_t len;
/** current pointer into buffer */
static uint8_t * cur;

static bool doConfig;

/** configuration */
static struct ADSB_CONFIG config;

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
	ADSB_OPTION_DF_11_17_18_CRC_ENABLED ='f',
	/** CRC: don't check DF-11/17/18 frames */
	ADSB_OPTION_DF_11_17_18_CRC_DISABLED ='F',

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
static inline void consume();
static inline bool peek(uint8_t * ch);
static inline bool next(uint8_t * ch);
static inline bool synchronize();
static inline bool setOption(enum ADSB_OPTION option);
static inline enum DECODE_STATUS decode(uint8_t * buf, size_t len,
	struct ADSB_Frame * frame);

void ADSB_init(const struct ADSB_CONFIG * cfg)
{
	INPUT_init();
	if (cfg) {
		doConfig = true;
		memcpy(&config, cfg, sizeof config);
	} else {
		doConfig = false;
	}
}

void ADSB_destruct()
{
	INPUT_destruct();
}

/** Setup ADSB receiver with some options. */
static bool configure()
{
	/* setup ADSB */
	return setOption(ADSB_OPTION_OUTPUT_FORMAT_BIN) &&
		setOption(config.frameFilter ?
			ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
			ADSB_OPTION_FRAME_FILTER_ALL) &&
		setOption(ADSB_OPTION_AVR_FORMAT_MLAT) &&
		setOption(config.crc ? ADSB_OPTION_DF_11_17_18_CRC_ENABLED :
			ADSB_OPTION_DF_11_17_18_CRC_DISABLED) &&
		setOption(config.timestampGPS ? ADSB_OPTION_TIMESTAMP_SOURCE_GPS :
			ADSB_OPTION_TIMESTAMP_SOURCE_LEGACY_12_MHZ) &&
		setOption(config.rtscts ? ADSB_OPTION_RTS_HANDSHAKE_ENABLED :
			ADSB_OPTION_RTS_HANDSHAKE_DISABLED) &&
		setOption(config.fec ? ADSB_OPTION_DF_17_18_FEC_ENABLED :
			ADSB_OPTION_DF_17_18_FEC_DISABLED) &&
		setOption(config.modeAC ? ADSB_OPTION_MODE_AC_DECODING_ENABLED :
			ADSB_OPTION_MODE_AC_DECODING_DISABLED);
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
		if (!doConfig || configure())
			break;
	}

	/* reset buffer */
	cur = buf;
	len = 0;
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
		uint8_t header[7];
		enum DECODE_STATUS rs = decode(header, sizeof header, frame);
		if (unlikely(rs == DECODE_STATUS_RESYNC))
			goto decode_frame;
		else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;
		/* decode mlat */
		uint64_t mlat = 0;
		memcpy(((uint8_t*)&mlat) + 2, header, 6);
		frame->mlat = be64toh(mlat);
		frame->siglevel = header[6];

		rs = decode(frame->payload, payload_len, frame);
		if (unlikely(rs == DECODE_STATUS_RESYNC))
			goto decode_frame;
		else if (unlikely(rs == DECODE_STATUS_CONNFAIL))
			return false;

		return true;
	}
}

/** Unescape frame payload.
 * \param dst destination buffer
 * \param len length of frame content to be decoded.
 *  Must not exceed the buffers size
 * \param rawPtrPtr pointer to pointer of raw buffer
 * \param lenRawPtr pointer to raw buffer length
 */
static inline enum DECODE_STATUS decode(uint8_t * dst, size_t len,
	struct ADSB_Frame * frame)
{
	size_t i;
	uint8_t * rawPtr = frame->raw + frame->raw_len;
	for (i = 0; i < len; ++i) {
		/* receive one character */
		uint8_t c;
		if (unlikely(!next(&c)))
			return DECODE_STATUS_CONNFAIL;
		/* put into buffer and raw buffer */
		*rawPtr++ = *dst++ = c;
		++frame->raw_len;
		if (unlikely(c == 0x1a)) { /* escaped character */
			if (unlikely(!peek(&c)))
				return DECODE_STATUS_CONNFAIL;
			if (unlikely(c == 0x1a)) {
				/* consume character */
				consume();
				/* put into raw buffer */
				*rawPtr++ = c;
				++frame->raw_len;
			} else {
				NOC_puts("ADSB: Out of Sync: got unescaped 0x1a in frame, "
					"treating as resynchronization");
				++STAT_stats.ADSB_outOfSync;
				return DECODE_STATUS_RESYNC;
			}
		}
	}
	return DECODE_STATUS_OK;
}

/** Discard buffer content and fill it again. */
static inline bool discardAndFill()
{
	size_t rc = INPUT_read(buf, sizeof buf);
	if (unlikely(rc == 0)) {
		return false;
	} else {
		len = rc;
		cur = buf;
		return true;
	}
}

static inline void consume()
{
	if (unlikely(cur >= buf + len))
		error(EXIT_FAILURE, 0, "ADSB: assertion cur >= buf + len violated");
	++cur;
}

/** Get next character from buffer but keep it there.
 * \return next character in buffer
 */
static inline bool peek(uint8_t * c)
{
	do {
		if (likely(cur < buf + len)) {
			*c = *cur;
			return true;
		}
	} while (likely(discardAndFill()));
	return false;
}

/** Consume next character from buffer.
 * \return next character in buffer
 */
static inline bool next(uint8_t * ch)
{
	bool ret = peek(ch);
	if (likely(ret))
		++cur;
	return ret;
}

/** Synchronize buffer.
 * \note After calling that, the next call to next() or peek() will return 0x1a.
 */
static inline bool synchronize()
{
	do {
		uint8_t * esc = memchr(cur, 0x1a, len - (cur - buf));
		if (likely(esc)) {
			cur = esc;
			return true;
		}
	} while (likely(discardAndFill()));
	return false;
}
