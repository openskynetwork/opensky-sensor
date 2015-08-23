#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include <adsb.h>
#include <component.h>

extern struct Component BUF_comp;

void BUF_flush();

void BUF_fillStatistics();

struct ADSB_Frame * BUF_newMessage();
struct ADSB_Frame * BUF_newMessageWait();
void BUF_commitMessage(struct ADSB_Frame * msg);
void BUF_abortMessage(struct ADSB_Frame * msg);

const struct ADSB_Frame * BUF_getMessage();
const struct ADSB_Frame * BUF_getMessageTimeout(uint32_t timeout_ms);
void BUF_releaseMessage(const struct ADSB_Frame * msg);
void BUF_putMessage(const struct ADSB_Frame * msg);

#endif
