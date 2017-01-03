/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_OPENSKYTYPES_H
#define _HAVE_OPENSKYTYPES_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Internal frame types, numbers must relate to the @BEAST_TYPE types */
enum OPENSKY_FRAME_TYPE {
	/** Mode-AC frame */
	OPENSKY_FRAME_TYPE_MODE_AC = 0,
	/** Mode-S short frame */
	OPENSKY_FRAME_TYPE_MODE_S_SHORT = 1,
	/** Mode-S Long frame */
	OPENSKY_FRAME_TYPE_MODE_S_LONG = 2,
	/** Status frame (sent by radarcape only) */
	OPENSKY_FRAME_TYPE_STATUS = 3,
};

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
