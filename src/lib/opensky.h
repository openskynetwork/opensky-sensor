/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_OPENSKY_H
#define _HAVE_OPENSKY_H

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Frame type */
enum OPENSKY_FRAME_TYPE {
	/** Mode-A/C */
	OPENSKY_FRAME_TYPE_MODE_AC = 0,
	/** Mode-S Short */
	OPENSKY_FRAME_TYPE_MODE_S_SHORT = 1,
	/** Mode-S Long */
	OPENSKY_FRAME_TYPE_MODE_S_LONG = 2,
	/** Radarcape Status (no longer needed if timestamp quality can be
	 * queried */
	OPENSKY_FRAME_TYPE_STATUS = 3,
};

/** Opensky frame */
struct OPENSKY_Frame {
	/** Frame type */
	enum OPENSKY_FRAME_TYPE frameType;

	/** mlat */
	uint64_t mlat;

	/** signal level */
	int8_t siglevel;

	/** payload length */
	size_t payloadLen;

	/** payload */
	uint8_t payload[14];
};

/** Change configuration.
 * By now, a standard configuration is loaded. Subject to change.
 */
void OPENSKY_configure();

/** Start streaming */
void OPENSKY_start();

/** Stop streaming */
void OPENSKY_stop();

/** Submit an Opensky frame.
 * \note Streaming must be startet, ie call OPENSKY_start() first.
 * \param frame opensky frame to be submitted.
 */
void OPENSKY_frame(const struct OPENSKY_Frame * frame);

#ifdef __cplusplus
}
#endif

#endif
