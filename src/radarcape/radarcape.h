/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_RADARCAPE_H
#define _HAVE_RADARCAPE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RADARCAPE_Statistics {
	uint64_t outOfSync;
	uint64_t frameTypeUnknown;
};

void RADARCAPE_getStatistics(struct RADARCAPE_Statistics * statistics);

#ifdef __cplusplus
}
#endif

#endif
