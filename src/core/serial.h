/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_SERIAL_H
#define _HAVE_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Serial number return codes */
enum SERIAL_RETURN {
	/** Temporary failure, try again
	 * TODO: this should be defined more precisely */
	SERIAL_RETURN_FAIL_TEMP,
	/** Network (to OpenSky) failed, try again */
	SERIAL_RETURN_FAIL_NET,
	/** Permanent failure */
	SERIAL_RETURN_FAIL_PERM,
	/** Success */
	SERIAL_RETURN_SUCCESS
};

enum SERIAL_RETURN SERIAL_getSerial(uint32_t * serial);

#ifdef __cplusplus
}
#endif

#endif
