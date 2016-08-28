/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_FILTER_H
#define _HAVE_FILTER_H

#include <stdbool.h>

#include "openskytypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void FILTER_init();
void FILTER_reset();
void FILTER_reconfigure(bool reset);
void FILTER_setSynchronized(bool synchronized);
bool FILTER_filter(enum OPENSKY_FRAME_TYPE frameType, uint8_t firstByte);

#ifdef __cplusplus
}
#endif

#endif
