/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_INPUT_PERF_H
#define _HAVE_INPUT_PERF_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "../openskytypes.h"

size_t RC_INPUT_buildFrame(uint8_t * buf, enum OPENSKY_FRAME_TYPE type,
	uint64_t mlat, int8_t siglevel, const char * payload, size_t payloadLen);

void RC_INPUT_setBuffer(uint8_t * buf, size_t length);

#endif
