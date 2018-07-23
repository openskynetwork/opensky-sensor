/* Copyright (c) 2015-2018 OpenSky Network <contact@opensky-network.org> */

#pragma once

#include <ostream>
#include <cstdint>
#include "devicetypes.h"

namespace OpenSky {

/** Call this function prior to any other function. It can be called during
 * the initialization phase of your application.
 * @note Do not call this more than once.
 */
void init();

/** Set or change configuration of the OpenSky feeder.
 * @param deviceType device type
 * @param serialNumber serial number of this device
 */
void configure(OPENSKY_DEVICE_TYPE deviceType, uint32_t serialNumber);

/** Set logging streams for the OpenSky feeder.
 * @param msgLog Message log stream
 * @param errLog Error log stream
 * @note default are std::cout and std::cerr, respectively
 */
void setLogStreams(std::ostream & msgLog, std::ostream & errLog);

/** Enable OpenSky feeder.
 * @note configure() should before enabling if the default configuration is not
 *  used.
 * @note setLogStreams() should be called before enabling.
 */
void enable();

/** Disable OpenSky feeder. */
void disable();

/** Set GPS position.
 * @param latitude latitude in degrees, as defined by WGS-84
 * @param longitude longitude in degrees, as defined by WGS-84
 * @param alitude altitude in meters */
void setGpsPosition(double latitude, double longitude, double altitude);

/** Unset GPS position, e.g. after losing the fix.
 * This is important that no invalid position information is sent after a
 * reconnect. */
void unsetGpsPosition();

/** Submit a frame to the OpenSky network.
 * @param timestamp timestamp as nanoseconds of week
 * @param timingUTC whether timestamp is relative to UTC
 * @param timingSync whether timestamp is synchronized by GNSS
 * @param rssi received signal strength indicator
 * @param payload frame payload
 * @param payloadLen frame length, either 7 or 14
 * @note The feeder must be enabled, lookup enable()
 */
void submitFrame(std::uint64_t timestamp, bool timingUTC, bool timingSync,
		std::int8_t rssi, const uint8_t * payload, std::size_t payloadLen);

}
