/* Copyright (c) 2015-2018 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_DEVICETYPES_H
#define _HAVE_DEVICETYPES_H

#ifdef __cplusplus
extern "C" {
#endif

/** Device types for extended beast protocol */
enum OPENSKY_DEVICE_TYPE {
	/** Invalid (i.e. unconfigured) */
	OPENSKY_DEVICE_TYPE_INVALID = 0,
	/** Bogus (sending random frames) */
	OPENSKY_DEVICE_TYPE_BOGUS = 1,
	/** Standalone radarcape */
	OPENSKY_DEVICE_TYPE_RADARCAPE = 2,
	/** Radarcape via network (e.g. Angstrom/debian package)*/
	OPENSKY_DEVICE_TYPE_RADARCAPE_NET = 3,
	/** Radarcape via library */
	OPENSKY_DEVICE_TYPE_RADARCAPE_LIB = 4,
	/** Dump1090 Feeder */
	OPENSKY_DEVICE_TYPE_FEEDER = 5,
	/** Dump1090 Feeder: Donated */
	OPENSKY_DEVICE_TYPE_FEEDER_DONATED = 6,
	/** Dump1090 Feeder: OpenSky Kit */
	OPENSKY_DEVICE_TYPE_OPENSKY_KIT = 7,
	/** Dump1090 Feeder: HPTOA branch */
	OPENSKY_DEVICE_TYPE_FEEDER_HPTOA = 8,
	/** SeRo Systems GRX Receiver */
	OPENSKY_DEVICE_TYPE_GRX = 9,
};

#ifdef __cplusplus
}
#endif

#endif
