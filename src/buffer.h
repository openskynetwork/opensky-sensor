/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include <adsb.h>
#include <component.h>

#ifdef __cplusplus
extern "C" {
#endif

extern struct Component BUF_comp;

void BUF_flush();

void BUF_runGC();

void BUF_fillStatistics();

struct ADSB_RawFrame * BUF_newFrame();
void BUF_commitFrame(struct ADSB_RawFrame * frame);
void BUF_abortFrame(struct ADSB_RawFrame * frame);

const struct ADSB_RawFrame * BUF_getFrame();
const struct ADSB_RawFrame * BUF_getFrameTimeout(uint_fast32_t timeout_ms);
void BUF_releaseFrame(const struct ADSB_RawFrame * frame);
void BUF_putFrame(const struct ADSB_RawFrame * frame);

#ifdef __cplusplus
}
#endif

#endif
