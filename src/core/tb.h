/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_TB_H
#define _HAVE_TB_H

#include <stdint.h>
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct Component TB_comp;

/** Talkback message identifiers */
enum TB_PACKET_TYPE {
	/** Open a reverse shell */
	TB_PACKET_TYPE_REVERSE_SHELL = 0,
	/** Restart daemon */
	TB_PACKET_TYPE_RESTART = 1,
	/** Reboot device (using systemd) */
	TB_PACKET_TYPE_REBOOT = 2,
	/** Upgrade daemon (using pacman) */
	TB_PACKET_TYPE_UPGRADE_DAEMON = 3,
	/** Set filter configuration */
	TB_PACKET_TYPE_FILTER = 4,
	/** Serial number response */
	TB_PACKET_TYPE_SERIAL_RES = 5,

	/** Number of packet types */
	TB_PACKET_TYPE_N
};

/** Packet processor callback
 * @param payload packet payload
 */
typedef void (*TB_ProcessorFn)(const uint8_t * payload);

void TB_register(uint32_t type, size_t payloadLen, TB_ProcessorFn fn);
void TB_unregister(uint32_t type);

#ifdef __cplusplus
}
#endif

#endif
