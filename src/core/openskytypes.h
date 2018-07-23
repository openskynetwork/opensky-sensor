/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_OPENSKYTYPES_H
#define _HAVE_OPENSKYTYPES_H

#include <stdlib.h>
#include <stdint.h>
#include "beast.h"
#include "devicetypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/** Constant for frame synchronization */
	OPENSKY_SYNC = BEAST_SYNC
};

/** Internal frame types, numbers must relate to the @BEAST_TYPE types */
enum OPENSKY_FRAME_TYPE {
	/** Mode-AC frame */
	OPENSKY_FRAME_TYPE_MODE_AC = BEAST_TYPE_MODE_AC,
	/** Mode-S short frame */
	OPENSKY_FRAME_TYPE_MODE_S_SHORT = BEAST_TYPE_MODE_S_SHORT,
	/** Mode-S Long frame */
	OPENSKY_FRAME_TYPE_MODE_S_LONG = BEAST_TYPE_MODE_S_LONG,
	/** Status frame (sent by radarcape only) */
	OPENSKY_FRAME_TYPE_STATUS = BEAST_TYPE_STATUS,

	/** Serial number */
	OPENSKY_FRAME_TYPE_SERIAL = '5',
	/** Keep alive */
	OPENSKY_FRAME_TYPE_KEEP_ALIVE = '6',
	/** GPS Position */
	OPENSKY_FRAME_TYPE_GPS_POSITION = '7',

	/** Device ID */
	OPENSKY_FRAME_TYPE_DEVICE_ID = 65,
	/** Serial number request */
	OPENSKY_FRAME_TYPE_SERIAL_REQ = 66,
	/** User name */
	OPENSKY_FRAME_TYPE_USER = 67,
};

#define OPENSKY_FRAME_TYPE_TO_INDEX(_ft) ((size_t)((_ft) - '1'))
#define OPENSKY_INDEX_TO_FRAME_TYPE(_idx) ((enum OPENSKY_FRAME_TYPE)((_idx) + '1'))

/** Maximal length of username. MUST match the server side */
#define OPENSKY_MAX_USERNAME 40

/** Decoded frame */
struct OPENSKY_DecodedFrame {
	/** Frame type */
	enum OPENSKY_FRAME_TYPE frameType;

	/** Timestamp, as encoded by the beast protocol */
	uint64_t mlat;
	/** Signal level, as defined by beast protocol */
	int8_t siglevel;

	/** Payload length */
	size_t payloadLen;
	/** Payload */
	uint8_t payload[14];
};

/** Raw frame */
struct OPENSKY_RawFrame {
	/** raw frame length */
	size_t rawLen;
	/** payload */
	uint8_t raw[23 * 2];
};

#ifdef __cplusplus
}
#endif

#endif
