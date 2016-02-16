/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_OPENSKY_H
#define _HAVE_OPENSKY_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum OPENSKY_FRAME_TYPE {
	OPENSKY_FRAME_TYPE_MODE_AC = 0,
	OPENSKY_FRAME_TYPE_MODE_S_SHORT = 1,
	OPENSKY_FRAME_TYPE_MODE_S_LONG = 2,
	OPENSKY_FRAME_TYPE_STATUS = 3,
};

struct OPENSKY_Frame {
	enum OPENSKY_FRAME_TYPE frameType;

	uint64_t mlat;
	int8_t siglevel;

	size_t payloadLen;
	uint8_t payload[14];
};

void OPENSKY_configure();

void OPENSKY_start();
void OPENSKY_stop();

void OPENSKY_frame(const struct OPENSKY_Frame * frame);

#ifdef __cplusplus
}
#endif

#endif
