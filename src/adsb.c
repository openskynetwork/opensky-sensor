
#define _DEFAULT_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <error.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
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

/** file descriptor for UART */
static int fd;
/** poll set when waiting for data */
static struct pollfd fds;
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


static inline void discardAndFill();
static inline char peek();
static inline char next();
static inline void synchronize();
static inline bool decode(uint8_t * dst, size_t len, uint8_t ** rawPtrPtr,
	size_t * lenRawPtr);
static inline void setOption(enum ADSB_OPTION option);

/** Initialize ADSB UART.
 * \param uart path to uart to be used
 * \param rtscts use RTS/CTS handshake on uart. Must be set in the receiver
 *  also.
 */
void ADSB_init(const char * uart, bool rtscts)
{
	/* open uart */
	fd = open(uart, O_RDWR, O_NONBLOCK | O_NOCTTY | O_CLOEXEC);
	if (fd < 0)
		error(-1, errno, "ADSB: Could not open UART at '%s'", uart);

	/* set uart options */
	struct termios t;
	if (tcgetattr(fd, &t) == -1)
		error(-1, errno, "ADSB: tcgetattr failed");

	t.c_iflag = IGNPAR;
	t.c_oflag = ONLCR;
	t.c_cflag = CS8 | CREAD | HUPCL | CLOCAL | B3000000;
	if (rtscts)
		t.c_cflag |= CRTSCTS;
	t.c_lflag &= ~(ISIG | ICANON | IEXTEN | ECHO);
	t.c_ispeed = B3000000;
	t.c_ospeed = B3000000;

	if (tcsetattr(fd, TCSANOW, &t))
		error(-1, errno, "ADSB: tcsetattr failed");

	tcflush(fd, TCIOFLUSH);

	/* setup polling */
	fds.events = POLLIN;
	fds.fd = fd;

	/* setup buffer */
	cur = buf;
	len = 0;

	/* setup filter */
	frameFilter = ADSB_FRAME_TYPE_ALL_MSGS;

	/* initialize synchronize info */
	isSynchronized = false;
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
static inline void setOption(enum ADSB_OPTION option)
{
	uint8_t w[3] = { '\x1a', '1', (uint8_t)option };
	write(fd, w, sizeof w);
}

/** ADSB main loop: receive, decode, buffer */
void ADSB_main()
{
	struct ADSB_Frame * frame = BUF_newFrame();
	while (1) {
		/* synchronize */
		while (1) {
			uint8_t sync = next();
			if (sync == 0x1a)
				break;
			fprintf(stderr, "ADSB: Out of Sync: got 0x%2d instead of 0x1a\n",
				sync);
			++STAT_stats.ADSB_outOfSync;
			synchronize();
		}

		/* decode frame */
		uint8_t * rawPtr;
decode_frame:
		rawPtr = frame->raw;
		*rawPtr++ = 0x1a;

		/* decode type */
		uint8_t type = next();
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
		*rawPtr++ = type;
		frame->frameType = 1 << (type - '1');
		frame->payload_len = payload_len;

		frame->raw_len = 2;

		/* decode header */
		uint8_t header[7];
		if (!decode(header, sizeof header, &rawPtr, &frame->raw_len))
			goto decode_frame;

		/* decode payload */
		if (!decode(frame->payload, payload_len, &rawPtr, &frame->raw_len))
			goto decode_frame;

		++STAT_stats.ADSB_frameType[type - '1'];

		/* process header */
		uint64_t mlat = 0;
		memcpy(((uint8_t*)&mlat) + 2, header, 6);
		frame->mlat = be64toh(mlat);
		frame->siglevel = header[6];

		if (frame->frameType == ADSB_FRAME_TYPE_STATUS)
			isSynchronized = frame->mlat != 0;

		/* apply filter */
		if (!(frame->frameType & frameFilter)) {
			++STAT_stats.ADSB_framesFiltered;
			continue;
		}

		/* filter if unsynchronized and filter is enabled*/
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

/** Returns whether the receiver is synchronized by GPS.
 * \return true if the receiver is synchronized, false otherwise
 */
bool ADSB_isSynchronized()
{
	return isSynchronized;
}

/** Decode frame content.
 * \param dst destination buffer
 * \param len length of frame content to be decoded.
 *  Must not exceed the buffers size
 * \param rawPtrPtr pointer to pointer of raw buffer
 * \param lenRawPtr pointer to raw buffer length
 */
static inline bool decode(uint8_t * dst, size_t len, uint8_t ** rawPtrPtr,
	size_t * lenRawPtr)
{
	size_t i;
	uint8_t * rawPtr = *rawPtrPtr;
	for (i = 0; i < len; ++i) {
		/* receive one character */
		uint8_t c = next();
		/* put into raw buffer */
		*rawPtr++ = c;
		++*lenRawPtr;
		if (c == 0x1a) { /* escaped character */
			c = peek();
			if (c == 0x1a) {
				/* consume character */
				next();
				/* put into raw buffer */
				*rawPtr++ = c;
				++*lenRawPtr;
				/* put unescaped character into buffer */
				*dst++ = 0x1a;
			} else {
				printf("ADSB: Out of Sync: got unescaped 0x1a in frame, "
					"treating as resynchronization\n");
				++STAT_stats.ADSB_outOfSync;
				return false;
			}
		} else {
			/* put into buffer */
			*dst++ = c;
		}
	}
	*rawPtrPtr = rawPtr;
	return true;
}

/** Discard buffer content and fill it again. */
static inline void discardAndFill()
{
	while (1) {
		int rc = read(fd, buf, sizeof buf);
		if (rc == -1) {
			if (errno == EAGAIN)
				poll(&fds, 1, -1);
			else
				error(-1, errno, "ADSB: read failed");
		} else if (rc == 0) {
			poll(&fds, 1, -1);
		} else {
			len = rc;
			break;
		}
	}

	cur = buf;
}

/** Get next character from buffer but keep it there.
 * \return next character in buffer
 */
static inline char peek()
{
	while (1) {
		if (cur < buf + len)
			return *cur;

		discardAndFill();
	}
}

/** Consume next character from buffer.
 * \return next character in buffer
 */
static inline char next()
{
	char ret = peek();
	++cur;
	return ret;
}

/** Synchronize buffer.
 * \note After calling that, the next call to next() or peek() will return 0x1a.
 */
static inline void synchronize()
{
	while (1) {
		uint8_t * esc = memchr(cur, 0x1a, len - (cur - buf));
		if (esc) {
			cur = esc;
			return;
		}
		discardAndFill();
	}
}
