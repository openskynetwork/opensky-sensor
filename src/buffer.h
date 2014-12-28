#ifndef _HAVE_FRAMEBUFFER_H
#define _HAVE_FRAMEBUFFER_H

#include <stdlib.h>
#include <adsb.h>

void FB_init(size_t backlog);
struct ADSB_Frame * FB_new();
void FB_put(struct ADSB_Frame * frame);

struct ADSB_Frame * FB_get(int32_t timeout_ms);
void FB_done(struct ADSB_Frame * frame);

#endif
