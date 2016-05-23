/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ADSB_FRAME_TYPE {
	ADSB_FRAME_TYPE_MODE_AC = 0,
	ADSB_FRAME_TYPE_MODE_S_SHORT = 1,
	ADSB_FRAME_TYPE_MODE_S_LONG = 2,
	ADSB_FRAME_TYPE_STATUS = 3,
};

struct ADSB_DecodedFrame {
	enum ADSB_FRAME_TYPE frameType;

	uint64_t mlat;
	int8_t siglevel;

	size_t payloadLen;
	uint8_t payload[14];
};

struct ADSB_RawFrame {
	size_t raw_len;
	uint8_t raw[23 * 2];
};

void ADSB_init();
void ADSB_destruct();

void ADSB_reconfigure();

void ADSB_connect();
bool ADSB_getFrame(struct ADSB_RawFrame * raw,
	struct ADSB_DecodedFrame * decoded);

#ifdef __cplusplus
}
#endif

#endif
