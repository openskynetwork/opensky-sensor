/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_SERIAL_H
#define _HAVE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool SERIAL_getSerial(uint32_t * serial);

#ifdef __cplusplus
}
#endif

#endif
