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

	/** Serial number */
	BEAST_TYPE_SERIAL = '5',
	/** Keep alive */
	BEAST_TYPE_KEEP_ALIVE = '6',
	/** GPS Position */
	BEAST_TYPE_GPS_POSITION = '7',

	/** Device ID */
	BEAST_TYPE_DEVICE_ID = 65,
	/** Serial number request */
	BEAST_TYPE_SERIAL_REQ = 66,
	/** User name */
	BEAST_TYPE_USER = 67,
};

/** Device types for extended beast protocol */
enum BEAST_DEVICE_TYPE {
	/** Invalid (i.e. unconfigured) */
	BEAST_DEVICE_TYPE_INVALID = 0,
	/** Bogus (sending random frames) */
	BEAST_DEVICE_TYPE_BOGUS = 1,
	/** Standalone radarcape */
	BEAST_DEVICE_TYPE_RADARCAPE = 2,
	/** Radarcape via network (e.g. Angstrom/debian package)*/
	BEAST_DEVICE_TYPE_RADARCAPE_NET = 3,
	/** Radarcape via library */
	BEAST_DEVICE_TYPE_RADARCAPE_LIB = 4,
	/** Dump1090 Feeder */
	BEAST_DEVICE_TYPE_FEEDER = 5,
	/** Windows Feeder */
	BEAST_DEVICE_TYPE_WIN_FEEDER = 6
};

/** Maximal length of username. MUST match the server side */
#define BEAST_MAX_USERNAME 40

size_t BEAST_encode(uint8_t * __restrict out, const uint8_t * __restrict in,
	size_t len);

#ifdef __cplusplus
}
#endif

#endif
