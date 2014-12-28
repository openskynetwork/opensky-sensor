#ifndef _HAVE_ADSB_H
#define _HAVE_ADSB_H

#include <stdlib.h>
#include <stdint.h>

struct ADSB_Frame {
	size_t raw_len;
	char raw[23 * 2];

	size_t payload_len;
	uint8_t type;
	uint64_t mlat;
	int8_t siglevel;
	char payload[14];

	struct ADSB_Frame * next;
};

void ADSB_init(const char * uart);
void ADSB_main();

int ADSB_getFrame(struct ADSB_Frame * frame, int32_t timeout_ms);

#endif
