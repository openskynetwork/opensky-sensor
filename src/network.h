/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_NETWORK_H
#define _HAVE_NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <adsb.h>
#include <component.h>
#include <gps.h>

extern struct Component NET_comp;

#ifdef __cplusplus
extern "C" {
#endif

void NET_waitConnected();

bool NET_sendFrame(const struct ADSB_RawFrame * frame);
bool NET_sendTimeout();
bool NET_sendPosition(const struct GPS_RawPosition * position);

ssize_t NET_receive(uint8_t * buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
