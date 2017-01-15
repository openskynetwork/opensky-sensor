/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_FILTER_H
#define _HAVE_FILTER_H

#include <stdbool.h>
#include <stdint.h>
#include "openskytypes.h"
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FILTER_Configuration {
	bool crc;
	bool sync;
	bool extSquitter;
};

struct FILTER_Statistics {
	uint64_t framesByType[4];
	uint64_t modeSByType[32];
	uint64_t unknown;
	uint64_t filtered;
	uint64_t modeSfiltered;
	uint64_t unsynchronized;
};

extern const struct Component FILTER_comp;

inline static const struct FILTER_Configuration * FILTER_getConfiguration()
{
	extern struct FILTER_Configuration FILTER_cfg;
	return &FILTER_cfg;
}

void FILTER_setSynchronizedFilter(bool syncFilter);
void FILTER_setModeSExtSquitter(bool modeSExtSquitter);

void FILTER_reset();
void FILTER_setSynchronized(bool synchronized);
bool FILTER_filter(enum OPENSKY_FRAME_TYPE frameType, uint8_t firstByte);

void FILTER_getStatistics(struct FILTER_Statistics * statistics);

#ifdef __cplusplus
}
#endif

#endif
