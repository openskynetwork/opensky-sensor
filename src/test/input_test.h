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

	uint64_t frmMlat;
	enum ADSB_FRAME_TYPE frmType;
	int8_t frmSigLevel;
	uint8_t * frmMsg;
	size_t frmMsgLen;
	uint8_t raw[23 * 2];
	size_t rawLen;
	bool frmRaw;
};

extern struct TEST test;

#endif
