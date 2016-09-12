/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_NETWORK_H
#define _HAVE_NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "component.h"
#include "gps.h"
#include "openskytypes.h"

extern const struct Component NET_comp;

#ifdef __cplusplus
extern "C" {
#endif

void NET_waitConnected();

bool NET_sendFrame(const struct OPENSKY_RawFrame * frame);
bool NET_sendTimeout();
bool NET_sendPosition(const struct GPS_RawPosition * position);

ssize_t NET_receive(uint8_t * buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
