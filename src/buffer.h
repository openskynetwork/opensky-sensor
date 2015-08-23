#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include <adsb.h>
#include <component.h>

extern struct Component BUF_comp;

void BUF_flush();

void BUF_fillStatistics();

struct ADSB_Frame * BUF_newFrame();
void BUF_commitFrame(struct ADSB_Frame * frame);
void BUF_abortFrame(struct ADSB_Frame * frame);

const struct ADSB_Frame * BUF_getFrame();
const struct ADSB_Frame * BUF_getFrameTimeout(uint32_t timeout_ms);
void BUF_releaseFrame(const struct ADSB_Frame * frame);
void BUF_putFrame(const struct ADSB_Frame * frame);

#endif
