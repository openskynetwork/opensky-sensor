/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_INPUT_H
#define _HAVE_GPS_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void GPS_INPUT_register();
void GPS_INPUT_init();
void GPS_INPUT_destruct();
void GPS_INPUT_connect();
size_t GPS_INPUT_read(uint8_t * buf, size_t bufLen);
size_t GPS_INPUT_write(const uint8_t * buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif
