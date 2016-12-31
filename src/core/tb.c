/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <endian.h>
#include "tb.h"
#include "filter.h"
#include "network.h"
#include "util/log.h"
#include "util/util.h"

/** Component: Prefix */
static const char PFX[] = "TB";

/* Packet format:
 * Format: |ID|Len|Payload|
 * Size:     2  2    var
 *
 * ID: packet type, encoded in big endian byte order
 * Len: length of packet in bytes, encoded in big endian byte order
 *      Max Length is 128 byte
 * Payload: packet payload, variable length
 *
 * Packet types:
 * * 0x0: start reverse shell
 *   Payload: - ip (encoded as 4 byte, big endian byte order)
 *            - port (encoded as 2 byte, big endian byte order)
 *   Size: 9
 * * 0x1: restart daemon, Size: 3
 * * 0x2: reboot, Size: 3
 * * 0x3: upgrade daemon and restart, Size: 3
 * * 0x4: upgrade system and reboot, Size: 3
 */

/** Packet header and payload pointer */
struct TB_Packet {
	/** frame type */
	uint_fast16_t type;
	/** frame length */
	uint_fast16_t len;

	/** pointer to payload */
	const uint8_t * payload;
};


/** Packet processor */
struct PacketProcessor {
	/** Callback to processor */
	TB_ProcessorFn fn;
	/** Expected payload length */
	size_t payloadLen;
};

static void mainloop();
static void processPacket(const struct TB_Packet * packet);
static void packetConfigureFilter(const uint8_t * payload);

/** All packet processors in order of their type */
static struct PacketProcessor processors[TB_PACKET_TYPE_N] = {
	[TB_PACKET_TYPE_FILTER] = { .payloadLen = 2, .fn = packetConfigureFilter }
};

/** Component: Descriptor */
const struct Component TB_comp = {
	.description = PFX,
	.main = &mainloop,
	.dependencies = { &NET_comp, NULL }
};

/** Register a packet processor
 * @param type packet type (id)
 * @param payloadLen expected payload length for this type
 * @param fn pointer to callback to be called upon that packet type
 */
void TB_register(uint32_t type, size_t payloadLen, TB_ProcessorFn fn)
{
	if (type < TB_PACKET_TYPE_N) {
		// assertion would be enough, but not for shitty old debian gcc
		struct PacketProcessor * processor = &processors[type];
		assert(!processor->fn);
		processor->payloadLen = payloadLen;
		processor->fn = fn;
	}
}

/** Unregister a packet processor
 * @param type packet type (id)
 */
void TB_unregister(uint32_t type)
{
	if (type < TB_PACKET_TYPE_N) {
		struct PacketProcessor * processor = &processors[type];
		assert(processor->fn);
		processor->fn = NULL;
	}
}

/** Main loop: receive from OpenSky Network, interpret and call processor. */
static void mainloop()
{
	uint8_t buf[128];
	size_t bufLen;

	while (true) {
		/* synchronize with receiver */
		NET_waitConnected();

		/* new connection, reset buffer */
		bufLen = 0;

		while (true) {
			while (bufLen >= 4) {
				/* enough data to read header */
				struct TB_Packet frame;
				uint16_t * buf16 = (uint16_t*)buf;
				frame.type = be16toh(buf16[0]);
				frame.len = be16toh(buf16[1]);
				if (frame.len > 128 || frame.len < 4) {
					/* sanity check failed, reset buffer */
					LOG_logf(LOG_LEVEL_WARN, PFX, "Wrong packet format "
						"(type=%" PRIuFAST16 ", len=%" PRIuFAST16 "), "
						"resetting buffer", frame.type, frame.len);
					bufLen = 0;
					break;
				}

				if (bufLen >= frame.len) {
					/* complete packet: process it */
					frame.payload = buf + 4;
					processPacket(&frame);
					/* and clear it from buffer */
					bufLen -= frame.len;
					if (bufLen)
						memmove(buf, buf + frame.len, bufLen);
				} else {
					/* packet incomplete: more data needed */
					break;
				}
			}

			/* read (more) data from network into buffer */
			ssize_t len = NET_receive(buf + bufLen, (sizeof buf) - bufLen);
			if (len <= 0) /* upon failure: wait for new connection */
				break;
			bufLen += len;
		}
	}
}

/** Process a packet: call handler if exists.
 * @param packet packet to be processed
 */
static void processPacket(const struct TB_Packet * packet)
{
	static const uint_fast32_t n_processors = ARRAY_SIZE(processors);
	const struct PacketProcessor * processor = &processors[packet->type];

	if (packet->type >= n_processors || !processor->fn) {
		/* packet type unknown */
		LOG_logf(LOG_LEVEL_WARN, PFX, "Unknown packet type (type=%"
			PRIuFAST16 ", len=%" PRIuFAST16 ")", packet->type, packet->len);
		return;
	} else {
		size_t payloadLen = packet->len - 4;
		if (payloadLen != processor->payloadLen) {
			/* expected length does not match -> discard */
			LOG_logf(LOG_LEVEL_WARN, PFX, "Packet of type %" PRIuFAST16 " has "
				"a size mismatch (payload len=%" PRIuFAST16 "), discarding",
				packet->type, payloadLen);
		} else {
			/* call processor */
			processor->fn(packet->payload);
		}
	}
}

/** Packet processor: reconfigure the filter
 * @param payload packet payload
 */
static void packetConfigureFilter(const uint8_t * payload)
{
	enum FILT {
		FILT_SYNC_ONLY = 1 << 0,
		FILT_EXT_SQUITTER_ONLY = 1 << 1,
		FILT_RESET_SYNC = 1 << 7
	};

	uint8_t mask = payload[0];
	uint8_t cfg = payload[1];
	if (mask & FILT_SYNC_ONLY) {
		LOG_logf(LOG_LEVEL_INFO, PFX, "Setting sync filter: %d",
					!!(cfg & FILT_SYNC_ONLY));
		FILTER_setSynchronizedFilter(!!(cfg & FILT_SYNC_ONLY));
	}
	if (mask & FILT_EXT_SQUITTER_ONLY) {
		LOG_logf(LOG_LEVEL_INFO, PFX, "Setting ext squitter only filter: %d",
			!!(cfg & FILT_EXT_SQUITTER_ONLY));
		FILTER_setModeSExtSquitter(!!(cfg & FILT_EXT_SQUITTER_ONLY));
	}
	if (mask & FILT_RESET_SYNC)
		FILTER_reset();
}

