/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_LOGIN_H
#define _HAVE_LOGIN_H

#ifdef __cplusplus
extern "C" {
#endif

bool LOGIN_login();

enum LOGIN_DEVICE_ID {
	LOGIN_DEVICE_ID_INVALID = 0,
	LOGIN_DEVICE_ID_BOGUS = 1,
	LOGIN_DEVICE_ID_RADARCAPE = 2,
	LOGIN_DEVICE_ID_RADARCAPE_NET = 3,
	LOGIN_DEVICE_ID_RADARCAPE_LIB = 4,
	LOGIN_DEVICE_ID_FEEDER = 5,
};

void LOGIN_setDeviceID(enum LOGIN_DEVICE_ID id);

#ifdef __cplusplus
}
#endif

#endif
