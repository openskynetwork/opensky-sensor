/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_TB_H
#define _HAVE_TB_H

#include <stdint.h>
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct Component TB_comp;

enum TB_PACKET_TYPE {
	TB_PACKET_TYPE_REVERSE_SHELL = 0,
	TB_PACKET_TYPE_RESTART,
	TB_PACKET_TYPE_REBOOT,
	TB_PACKET_TYPE_UPGRADE_DAEMON,
	TB_PACKET_TYPE_FILTER,

	TB_PACKET_TYPE_N
};

/** Packet processor callback */
typedef void (*TB_ProcessorFn)(const uint8_t *);

void TB_register(uint32_t type, size_t payloadLen, TB_ProcessorFn fn);

#ifdef __cplusplus
}
#endif

#endif
