#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

enum ADSB_FRAME_TYPE {
	ADSB_FRAME_TYPE_MODE_AC = 0,
	ADSB_FRAME_TYPE_MODE_S_SHORT = 1,
	ADSB_FRAME_TYPE_MODE_S_LONG = 2,
	ADSB_FRAME_TYPE_STATUS = 3,
};

struct ADSB_Frame {
	enum ADSB_FRAME_TYPE frameType;

	uint64_t mlat;
	int8_t siglevel;

	size_t payloadLen;
	uint8_t payload[14];

	size_t raw_len;
	uint8_t raw[23 * 2];
};

struct ADSB_Header {
	uint64_t mlat;
	int8_t siglevel;
};

struct ADSB_CONFIG {
	bool frameFilter;
	bool crc;
	bool timestampGPS;
	bool rtscts;
	bool fec;
	bool modeAC;
};

void ADSB_init(const struct ADSB_CONFIG * cfg);
void ADSB_destruct();

void ADSB_connect();
bool ADSB_getFrame(struct ADSB_Frame * frame);

#endif
