/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_FILTER_H
#define _HAVE_FILTER_H

#include <stdbool.h>
#include <adsb.h>

void FILTER_init();
void FILTER_reset();
bool FILTER_filter(const struct ADSB_Frame * frame);

#endif
