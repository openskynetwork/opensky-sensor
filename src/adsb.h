#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct ADSB_Frame {
	size_t raw_len;
	uint8_t raw[23 * 2];

	size_t payload_len;
	uint8_t type;
	uint64_t mlat;
	int8_t siglevel;
	uint8_t payload[14];
};

void ADSB_init(const char * uart, bool rtscts);
void ADSB_main();
void ADSB_setup(bool outputFormatBin, bool avrMLAT, bool crc,
	bool fec, bool frameFilter, bool modeAC, bool rts, bool gps);

#endif
