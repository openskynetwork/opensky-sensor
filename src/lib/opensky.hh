
#pragma once

#include <ostream>
#include <MessageTypes.h>

typedef enum {
	GpsTimeInvalid,
	UsingCpuTime,
	UsingGpsTime
} GpsTimeStatus_t;

namespace OpenSky {

/** Set or change configuration of the OpenSky feeder.
 * \note TODO: Agree on parameters
 */
void configure();

/** Set logging streams for the OpenSky feeder.
 * \param msgLog Message log stream
 * \param errLog Error log stream
 */
void setLogStreams(std::ostream & msgLog, std::ostream & errLog);

/** Enable OpenSky feeder.
 * \note OPENSKY_configure() must be called at least once before enabling.
 * \note OPENSKY_setLogStreams() must be called at least once before enabling.
 */
void enable();

/** Disable OpenSky feeder. */
void disable();

void setGpsTimeStatus(const GpsTimeStatus_t gpsTimeStatus);

/** Submit a frame to the OpenSky network.
 * \param msg Message, consisting of timestamp (6 byte, big endian),
 *  signal level (1 byte, signed), payload (length depends on messageType)
 * \param messageType Type of msg.
 * \note The feeder must be enabled, lookup OPENSKY_enable()
 */
void output_message(const unsigned char * const msg,
	const enum MessageType_T messageType);

}
