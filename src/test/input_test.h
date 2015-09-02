#ifndef _HAVE_INPUT_TEST_H
#define _HAVE_INPUT_TEST_H

#include <adsb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
	bool params[8];


	int32_t testAck;

	uint32_t curBuffer;
	struct TEST_Buffer * buffers;
	uint32_t nBuffers;
};

extern struct TEST test;

#endif
