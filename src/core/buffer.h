/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include "util/component.h"
#include "openskytypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool BUF_cfgHistory;

extern const struct Component BUF_comp;

void BUF_flush();

void BUF_runGC();

void BUF_fillStatistics();

struct OPENSKY_RawFrame * BUF_newFrame();
void BUF_commitFrame(struct OPENSKY_RawFrame * frame);
void BUF_abortFrame(struct OPENSKY_RawFrame * frame);

const struct OPENSKY_RawFrame * BUF_getFrame();
const struct OPENSKY_RawFrame * BUF_getFrameTimeout(uint_fast32_t timeout_ms);
void BUF_releaseFrame(const struct OPENSKY_RawFrame * frame);
void BUF_putFrame(const struct OPENSKY_RawFrame * frame);

#ifdef __cplusplus
}
#endif

#endif
