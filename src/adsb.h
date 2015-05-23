#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <component.h>

enum ADSB_FRAME_TYPE {
	ADSB_FRAME_TYPE_NONE = 0,

	ADSB_FRAME_TYPE_MODE_AC = 1 << 0,
	ADSB_FRAME_TYPE_MODE_S_SHORT = 1 << 1,
	ADSB_FRAME_TYPE_MODE_S_LONG = 1 << 2,
	ADSB_FRAME_TYPE_STATUS = 1 << 3,

	ADSB_FRAME_TYPE_ALL = ADSB_FRAME_TYPE_MODE_AC |
		ADSB_FRAME_TYPE_MODE_S_SHORT |
		ADSB_FRAME_TYPE_MODE_S_LONG
};

enum ADSB_LONG_FRAME_TYPE {
	ADSB_LONG_FRAME_TYPE_NONE = 0,

	ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER = 1 << 17,
	ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	ADSB_LONG_FRAME_TYPE_ALL = ~0,
	ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL =
		ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER |
		ADSB_LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
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

extern struct Component ADSB_comp;

void ADSB_setFilter(enum ADSB_FRAME_TYPE frameType);
void ADSB_setFilterLong(enum ADSB_LONG_FRAME_TYPE frameLongType);
void ADSB_setSynchronizationFilter(bool enable);

bool ADSB_isSynchronized();

#endif
