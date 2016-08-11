/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#pragma once

#include <ostream>
#include <MessageTypes.h>
#include <GpsStatus.h>

namespace OpenSky {

void init();

/** Set or change configuration of the OpenSky feeder.
 */
void configure();

/** Set logging streams for the OpenSky feeder.
 * \param msgLog Message log stream
 * \param errLog Error log stream
 */
void setLogStreams(std::ostream & msgLog, std::ostream & errLog);

/** Enable OpenSky feeder.
 * \note configure() should before enabling if the default configuration is not
 *  used.
 * \note setLogStreams() should before enabling.
 */
void enable();

/** Disable OpenSky feeder. */
void disable();

/** Set the GPS Time Status.
 * \param gpsTimeStatus time status as given by GPS component
 */
void setGpsTimeStatus(const GpsTimeStatus_t gpsTimeStatus);

/** Set the GPS position.
 * \param latitude latitude in degrees, as defined by WGS-84
 * \param longitude longitude in degrees, as defined by WGS-84
 * \param alitude altitude in meters */
void setGpsPosition(double latitude, double longitude, double altitude);

/** Submit a frame to the OpenSky network.
 * \param msg Message, consisting of timestamp (6 byte, big endian),
 *  signal level (1 byte, signed), payload (length depends on messageType)
 * \param messageType Type of msg.
 * \note The feeder must be enabled, lookup enable()
 */
void output_message(const unsigned char * const msg,
	const enum MessageType_T messageType);

}
