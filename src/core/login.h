/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_LOGIN_H
#define _HAVE_LOGIN_H

#include "openskytypes.h"

#ifdef __cplusplus
extern "C" {
#endif

bool LOGIN_login();

void LOGIN_setDeviceType(enum OPENSKY_DEVICE_TYPE type);
void LOGIN_setUsername(const char * username);

#ifdef __cplusplus
}
#endif

#endif
