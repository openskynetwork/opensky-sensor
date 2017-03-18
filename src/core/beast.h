/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_BEAST_H
#define _HAVE_BEAST_H

#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/** Constant for frame synchronization */
	BEAST_SYNC = '\x1a'
};

/** Message identifiers for extended beast protocol */
enum BEAST_TYPE {
	/** Mode AC frame */
	BEAST_TYPE_MODE_AC = '1',
	/** Mode-S Short frame */
	BEAST_TYPE_MODE_S_SHORT = '2',
	/** Mode-S Long frame */
	BEAST_TYPE_MODE_S_LONG = '3',
	/** Status frame (sent by radarcape only) */
	BEAST_TYPE_STATUS = '4',
};

size_t BEAST_encode(uint8_t * __restrict out, const uint8_t * __restrict in,
	size_t len);

#ifdef __cplusplus
}
#endif

#endif
