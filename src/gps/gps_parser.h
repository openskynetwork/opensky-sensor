/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPS_PARSER_H
#define _HAVE_GPS_PARSER_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void GPS_PARSER_init();
void GPS_PARSER_destruct();

void GPS_PARSER_connect();
size_t GPS_PARSER_getFrame(uint8_t * buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif
