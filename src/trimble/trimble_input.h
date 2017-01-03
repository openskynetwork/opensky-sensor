/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_INPUT_H
#define _HAVE_GPS_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TRIMBLE_INPUT_register();
void TRIMBLE_INPUT_init();
void TRIMBLE_INPUT_disconnect();
void TRIMBLE_INPUT_connect();
size_t TRIMBLE_INPUT_read(uint8_t * buf, size_t bufLen);
size_t TRIMBLE_INPUT_write(const uint8_t * buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif
