/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_FILTER_H
#define _HAVE_FILTER_H

#include <stdbool.h>
#include <adsb.h>

#ifdef __cplusplus
extern "C" {
#endif

void FILTER_init();
void FILTER_reset();
bool FILTER_filter(const struct ADSB_Frame * frame);

#ifdef __cplusplus
}
#endif

#endif
