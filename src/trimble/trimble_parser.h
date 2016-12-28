/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_PARSER_H
#define _HAVE_GPS_PARSER_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TRIMBLE_PARSER_init();

void TRIMBLE_PARSER_connect();
void TRIMBLE_PARSER_disconnect();
size_t TRIMBLE_PARSER_getFrame(uint8_t * buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif
