/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_SERIAL_ETH_H
#define _HAVE_SERIAL_ETH_H

#include <stdbool.h>
#include <stdint.h>
#include "core/serial.h"
#include "component.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct Component SERIAL_comp;

enum SERIAL_RETURN SERIAL_ETH_getSerial(uint32_t * serial);

#ifdef __cplusplus
}
#endif

#endif
