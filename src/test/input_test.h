/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_INPUT_TEST_H
#define _HAVE_INPUT_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "core/openskytypes.h"

struct TEST_Buffer {
	uint8_t * payload;
	size_t length;
};

struct TEST {
	bool init;
	bool destruct;
	uint32_t connect;
	uint32_t write;
	uint32_t write_bytes;
	bool params[26];

	int32_t testAck;

	uint32_t curBuffer;
	struct TEST_Buffer * buffers;
	uint32_t nBuffers;
	bool noRet;
};

extern struct TEST test;

size_t RC_INPUT_buildFrame(uint8_t * buf, enum OPENSKY_FRAME_TYPE type,
	uint64_t mlat, int8_t siglevel, const char * payload, size_t payloadLen);

#endif
