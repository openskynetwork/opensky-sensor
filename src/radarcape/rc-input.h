/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_RC_INPUT_H
#define _HAVE_RC_INPUT_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void RC_INPUT_register();
void RC_INPUT_init();
void RC_INPUT_disconnect();
void RC_INPUT_connect();
size_t RC_INPUT_read(uint8_t * buf, size_t bufLen);
size_t RC_INPUT_write(uint8_t * buf, size_t bufLen);

#ifdef __cplusplus
}
#endif

#endif
