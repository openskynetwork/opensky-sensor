#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <cfgfile.h>

enum ADSB_FRAME_TYPE {
	ADSB_FRAME_TYPE_NONE = 0,

	ADSB_FRAME_TYPE_MODE_AC = 1 << 0,
	ADSB_FRAME_TYPE_MODE_S_SHORT = 1 << 1,
	ADSB_FRAME_TYPE_MODE_S_LONG = 1 << 2,
	ADSB_FRAME_TYPE_STATUS = 1 << 3,

	ADSB_FRAME_TYPE_ALL_MSGS = ADSB_FRAME_TYPE_MODE_AC |
		ADSB_FRAME_TYPE_MODE_S_SHORT |
		ADSB_FRAME_TYPE_MODE_S_LONG
};

struct ADSB_Frame {
	size_t raw_len;
	uint8_t raw[23 * 2];

	enum ADSB_FRAME_TYPE frameType;
};

struct ADSB_Header {
	uint64_t mlat;
	int8_t siglevel;
};

void ADSB_init(const struct CFG_ADSB * cfg);
void ADSB_setup(bool crc, bool fec, bool frameFilter, bool modeAC, bool rts,
	bool gps);
void ADSB_main();

void ADSB_setFilter(enum ADSB_FRAME_TYPE frameType);
void ADSB_setSynchronizationFilter(bool enable);

bool ADSB_isSynchronized();

#endif
