#ifndef _HAVE_INPUT_TEST_H
#define _HAVE_INPUT_TEST_H

#include <adsb.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct TEST {
	bool init;
	bool destruct;
	uint32_t connect;
	uint32_t write;
	uint32_t write_bytes;
	bool params[8];


	int32_t testAck;

	bool hasRead;

	uint8_t * frmMsg;
	size_t frmMsgLen;
};

extern struct TEST test;

#endif
