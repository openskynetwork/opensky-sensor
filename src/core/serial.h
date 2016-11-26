/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_SERIAL_H
#define _HAVE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum SERIAL_RETURN {
	SERIAL_RETURN_FAIL_TEMP,
	SERIAL_RETURN_FAIL_NET,
	SERIAL_RETURN_FAIL_PERM,
	SERIAL_RETURN_SUCCESS
};

enum SERIAL_RETURN SERIAL_getSerial(uint32_t * serial);

#ifdef __cplusplus
}
#endif

#endif
