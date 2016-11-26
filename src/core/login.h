/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_LOGIN_H
#define _HAVE_LOGIN_H

#ifdef __cplusplus
extern "C" {
#endif

bool LOGIN_login();

enum LOGIN_DEVICE_TYPE {
	LOGIN_DEVICE_TYPE_INVALID = 0,
	LOGIN_DEVICE_TYPE_BOGUS = 1,
	LOGIN_DEVICE_TYPE_RADARCAPE = 2,
	LOGIN_DEVICE_TYPE_RADARCAPE_NET = 3,
	LOGIN_DEVICE_TYPE_RADARCAPE_LIB = 4,
	LOGIN_DEVICE_TYPE_FEEDER = 5,
};

void LOGIN_setDeviceType(enum LOGIN_DEVICE_TYPE id);
void LOGIN_setUsername(const char * username);

#ifdef __cplusplus
}
#endif

#endif
