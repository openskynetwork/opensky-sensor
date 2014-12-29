#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <adsb.h>

void BUF_init(size_t backlog);
void BUF_setFilter(uint8_t frameType);

struct ADSB_Frame * BUF_prepareFrame();
void BUF_putFrame(struct ADSB_Frame * frame);

struct ADSB_Frame * BUF_getFrame();
struct ADSB_Frame * BUF_getFrameTimeout(uint32_t timeout_ms);
void BUF_releaseFrame(struct ADSB_Frame * frame);

#endif
