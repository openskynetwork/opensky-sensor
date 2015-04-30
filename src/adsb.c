
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
#include <buffer.h>
#include <statistics.h>
#include <input.h>

/** receive buffer */
static uint8_t buf[128];
/** current buffer length (max sizeof buf) */
static size_t len;
/** current pointer into buffer */
static uint8_t * cur;

/** frame filter */
static enum ADSB_FRAME_TYPE frameFilter;

/** frame filter */
static bool synchronizationFilter;

/** synchronization info: true if receiver has a valid GPS timestamp */
static bool isSynchronized;

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

enum RAW_STATUS {
	RAW_STATUS_OK,
	RAW_STATUS_RESYNC,
	RAW_STATUS_CONNFAIL
};

static inline bool discardAndFill();
static inline void consume();
static inline bool peek(uint8_t * ch);
static inline bool next(uint8_t * ch);
static inline bool synchronize();
static inline bool setOption(enum ADSB_OPTION option);
static inline void receiveFrames();
static inline enum RAW_STATUS readRaw(size_t len, struct ADSB_Frame * frame);
static inline void decodeHeader(const struct ADSB_Frame * frame,
	struct ADSB_Header * header);

/** Initialize ADSB UART.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 */
void ADSB_init(const struct CFG_ADSB * cfg)
{
	/* setup filter */
	frameFilter = ADSB_FRAME_TYPE_ALL_MSGS;

	/* initialize synchronize info */
	isSynchronized = false;

	INPUT_init(cfg);
}

/** Setup ADSB receiver with some options.
 * \param crc enable CRC
 * \param fec enable forward error correction
 * \param frameFilter receive DF-11/17/18 frames only
 * \param modeAC receive Mode-A/C additionally
 * \param rts use RTS/CTS handshake on UART
 * \param gps use GPS for timestamps
 */
void ADSB_setup(bool crc, bool fec, bool frameFilter, bool modeAC, bool rts,
	bool gps)
{
	/* setup ADSB */
	setOption(ADSB_OPTION_OUTPUT_FORMAT_BIN);
	setOption(frameFilter ? ADSB_OPTION_FRAME_FILTER_DF_11_17_18_ONLY :
		ADSB_OPTION_FRAME_FILTER_ALL);
	setOption(ADSB_OPTION_AVR_FORMAT_MLAT);
	setOption(crc ? ADSB_OPTION_DF_11_17_18_CRC_ENABLED :
		ADSB_OPTION_DF_11_17_18_CRC_DISABLED);
	setOption(gps ? ADSB_OPTION_TIMESTAMP_SOURCE_GPS :
		ADSB_OPTION_TIMESTAMP_SOURCE_LEGACY_12_MHZ);
	setOption(rts ? ADSB_OPTION_RTS_HANDSHAKE_ENABLED :
		ADSB_OPTION_RTS_HANDSHAKE_DISABLED);
	setOption(fec ? ADSB_OPTION_DF_17_18_FEC_ENABLED :
		ADSB_OPTION_DF_17_18_FEC_DISABLED);
	setOption(modeAC ? ADSB_OPTION_MODE_AC_DECODING_ENABLED :
		ADSB_OPTION_MODE_AC_DECODING_DISABLED);
}

/** Set a filter for all received frames.
 * \param frameType types which pass the filter, or'ed together
 *  All other frames will be discarded.
 * \note This is a software filter. If frames are discarded by the receiver
 *  already, they won't show here. So if Mode-A/C messages should pass, be also
 *  sure to not filter them with ADSB_setup
 */
void ADSB_setFilter(enum ADSB_FRAME_TYPE frameType)
{
	frameFilter = frameType;
}

/** Filter all frames (except status frames unless they're filtered anyway)
 * until the receiver is synchronized.
 * \param enable true to enable the synchronization filter, false to disable it
 */
void ADSB_setSynchronizationFilter(bool enable)
{
	synchronizationFilter = enable;
}

/** Set an option for the ADSB decoder.
 * \param option option to be set
 */
static inline bool setOption(enum ADSB_OPTION option)
{
	uint8_t w[3] = { '\x1a', '1', (uint8_t)option };
	return INPUT_write(w, sizeof w);
}

/** ADSB main loop: receive, decode, buffer */
void ADSB_main()
{
	while (true) {
		/* connect with input */
		INPUT_connect();

		/* reset buffer */
		cur = buf;
		len = 0;

		receiveFrames();

		/* TODO: sleep */
	}
}

static inline void receiveFrames()
{
	struct ADSB_Frame * frame = BUF_newFrame();
	while (true) {
		/* synchronize */
		while (true) {
			uint8_t sync;
			if (!next(&sync)) {
				BUF_abortFrame(frame);
				return;
			}
			if (sync == 0x1a)
				break;
			fprintf(stderr, "ADSB: Out of Sync: got 0x%2d instead of 0x1a\n",
				sync);
			++STAT_stats.ADSB_outOfSync;
			synchronize();
		}

		/* decode frame */
		frame->raw[0] = 0x1a;
		uint8_t type;
decode_frame:
		/* decode type */
		if (!next(&type)) {
			BUF_abortFrame(frame);
			return;
		}
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
		default:
			fprintf(stderr, "ADSB: Unknown frame type %c, resynchronizing\n",
				type);
			++STAT_stats.ADSB_frameTypeUnknown;
			synchronize();
			continue;
		}
		frame->raw[1] = type;
		frame->frameType = 1 << (type - '1');
		frame->raw_len = 2;

		/* read header */
		enum RAW_STATUS rs = readRaw(7, frame);
		if (rs == RAW_STATUS_RESYNC)
			goto decode_frame;
		else if (rs == RAW_STATUS_CONNFAIL)
			return;

		/* read payload */
		rs = readRaw(payload_len, frame);
		if (rs == RAW_STATUS_RESYNC)
			goto decode_frame;
		else if (rs == RAW_STATUS_CONNFAIL)
			return;

		++STAT_stats.ADSB_frameType[type - '1'];

		if (frame->frameType == ADSB_FRAME_TYPE_STATUS) {
			struct ADSB_Header header;
			decodeHeader(frame, &header);
			isSynchronized = header.mlat != 0;
		}

		/* apply filter */
		if (!(frame->frameType & frameFilter)) {
			++STAT_stats.ADSB_framesFiltered;
			continue;
		}

		/* filter if unsynchronized and filter is enabled */
		if (!isSynchronized) {
			++STAT_stats.ADSB_framesUnsynchronized;
			if (synchronizationFilter &&
				frame->frameType != ADSB_FRAME_TYPE_STATUS)
				continue;
		}

		/* buffer frame */
		BUF_commitFrame(frame);
		frame = BUF_newFrame();
	}
}

static inline void decodeHeader(const struct ADSB_Frame * frame,
	struct ADSB_Header * header)
{
	uint8_t decoded[7];

	size_t i;
	const uint8_t * rawHeader = frame->raw + 2;
	for (i = 0; i < sizeof decoded; ++i)
		if ((decoded[i] = *rawHeader++) == 0x1a)
			++rawHeader;

	/* decode mlat */
	uint64_t mlat = 0;
	memcpy(((uint8_t*)&mlat) + 2, decoded, 6);
	header->mlat = be64toh(mlat);
	header->siglevel = decoded[6];
}

/** Unescape frame payload.
 * \param dst destination buffer
 * \param len length of frame content to be decoded.
 *  Must not exceed the buffers size
 * \param rawPtrPtr pointer to pointer of raw buffer
 * \param lenRawPtr pointer to raw buffer length
 */
static inline enum RAW_STATUS readRaw(size_t len, struct ADSB_Frame * frame)
{
	size_t i;
	uint8_t * rawPtr = frame->raw + frame->raw_len;
	for (i = 0; i < len; ++i) {
		/* receive one character */
		uint8_t c;
		if (!next(&c))
			return RAW_STATUS_CONNFAIL;
		/* put into raw buffer */
		*rawPtr++ = c;
		++frame->raw_len;
		if (c == 0x1a) { /* escaped character */
			if (!peek(&c))
				return RAW_STATUS_CONNFAIL;
			if (c == 0x1a) {
				/* consume character */
				consume();
				/* put into raw buffer */
				*rawPtr++ = c;
				++frame->raw_len;
			} else {
				printf("ADSB: Out of Sync: got unescaped 0x1a in frame, "
					"treating as resynchronization\n");
				++STAT_stats.ADSB_outOfSync;
				return RAW_STATUS_RESYNC;
			}
		}
	}
	return RAW_STATUS_OK;
}

/** Returns whether the receiver is synchronized by GPS.
 * \return true if the receiver is synchronized, false otherwise
 */
bool ADSB_isSynchronized()
{
	return isSynchronized;
}

/** Discard buffer content and fill it again. */
static inline bool discardAndFill()
{
	size_t rc = INPUT_read(buf, sizeof buf);
	if (rc == 0) {
		return false;
	} else {
		len = rc;
		cur = buf;
		return true;
	}
}

static inline void consume()
{
	if (cur >= buf + len)
		error(EXIT_FAILURE, 0, "ADSB: assertion cur >= buf + len violated");
	++cur;
}

/** Get next character from buffer but keep it there.
 * \return next character in buffer
 */
static inline bool peek(uint8_t * c)
{
	do {
		if (cur < buf + len) {
			*c = *cur;
			return true;
		}
	} while (discardAndFill());
	return false;
}

/** Consume next character from buffer.
 * \return next character in buffer
 */
static inline bool next(uint8_t * ch)
{
	bool ret = peek(ch);
	if (ret)
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
		if (esc) {
			cur = esc;
			return true;
		}
	} while (discardAndFill());
	return false;
}
