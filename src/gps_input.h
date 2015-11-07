/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_GPS_INPUT_H
#define _HAVE_GPS_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

void GPS_INPUT_init();
void GPS_INPUT_destruct();
void GPS_INPUT_connect();
size_t GPS_INPUT_read(uint8_t * buf, size_t bufLen);
size_t GPS_INPUT_write(const uint8_t * buf, size_t bufLen);

#endif
