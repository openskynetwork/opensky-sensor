/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_TB_MISC_H
#define _HAVE_TB_MISC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TB_restartDaemon(const uint8_t * payload);

void TB_rebootSystem(const uint8_t * payload);

void TB_upgradeDaemon(const uint8_t * payload);

#ifdef __cplusplus
}
#endif

#endif
