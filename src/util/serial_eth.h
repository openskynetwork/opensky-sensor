/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_SERIAL_ETH_H
#define _HAVE_SERIAL_ETH_H

#include <stdbool.h>
#include <stdint.h>
#include "core/serial.h"

#ifdef __cplusplus
extern "C" {
#endif

enum SERIAL_RETURN SERIAL_ETH_getSerial(uint32_t * serial);

#ifdef __cplusplus
}
#endif

#endif
