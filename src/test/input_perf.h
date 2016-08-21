/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_INPUT_TEST_H

#define _HAVE_INPUT_TEST_H

#include <openskytypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

size_t INPUT_buildFrame(uint8_t * buf, enum OPENSKY_FRAME_TYPE type,
	uint64_t mlat, int8_t siglevel, const char * payload, size_t payloadLen);

void INPUT_setBuffer(uint8_t * buf, size_t length);

#endif
