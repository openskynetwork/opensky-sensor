#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

enum ADSB_FRAME_TYPE {
	ADSB_FRAME_TYPE_NONE = 0,

	ADSB_FRAME_TYPE_MODE_AC = 1 << 0,
	ADSB_FRAME_TYPE_MODE_S_SHORT = 1 << 1,
	ADSB_FRAME_TYPE_MODE_S_LONG = 1 << 2,

	ADSB_FRAME_TYPE_ALL = ADSB_FRAME_TYPE_MODE_AC |
		ADSB_FRAME_TYPE_MODE_S_SHORT |
		ADSB_FRAME_TYPE_MODE_S_LONG
};

struct ADSB_Frame {
	size_t raw_len;
	uint8_t raw[23 * 2];

	size_t payload_len;
	enum ADSB_FRAME_TYPE frameType;
	uint64_t mlat;
	int8_t siglevel;
	uint8_t payload[14];
};

void ADSB_init(const char * uart, bool rtscts);
void ADSB_main();
void ADSB_setup(bool crc, bool fec, bool frameFilter, bool modeAC, bool rts,
	bool gps);
void ADSB_setFilter(enum ADSB_FRAME_TYPE frameType);

#endif
