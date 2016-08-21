/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_OPENSKYTYPES_H
#define _HAVE_OPENSKYTYPES_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum OPENSKY_FRAME_TYPE {
	OPENSKY_FRAME_TYPE_MODE_AC = 0,
	OPENSKY_FRAME_TYPE_MODE_S_SHORT = 1,
	OPENSKY_FRAME_TYPE_MODE_S_LONG = 2,
	OPENSKY_FRAME_TYPE_STATUS = 3,
};

struct OPENSKY_DecodedFrame {
	enum OPENSKY_FRAME_TYPE frameType;

	uint64_t mlat;
	int8_t siglevel;

	size_t payloadLen;
	uint8_t payload[14];
};

struct OPENSKY_RawFrame {
	size_t raw_len;
	uint8_t raw[23 * 2];
};

#ifdef __cplusplus
}
#endif

#endif
