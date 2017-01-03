/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#include <cstring>
#include <unistd.h>
#include <endian.h>
#include <stdint.h>
#include "opensky.hh"

int main()
{
	OpenSky::init();

	//OpenSky::configure();

	OpenSky::enable();

	OpenSky::setGpsTimeStatus(UsingGpsTime);

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	unsigned char frame[6 + 1 + 14];
	std::memset(frame, 0x0, sizeof frame);

	uint64_t mlat = htobe64(123456789012345);
	std::memcpy(frame, &mlat, 6);
	frame[6] = 100;
	frame[7] = 0x8a;

	uint32_t i;
	for (i = 0; i < 10; ++i) {
		frame[8] = i;
		OpenSky::output_message(frame, MessageType_ModeSLong);
	}

	sleep(1);
	OpenSky::setGpsPosition(4.3, 140.5, 250.);

	sleep(3);

	for (i = 0; i < 10; ++i) {
		frame[8] = i + 10;
		OpenSky::output_message(frame, MessageType_ModeSLong);
	}

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	OpenSky::disable();

	sleep(2);

	OpenSky::enable();
	OpenSky::setGpsTimeStatus(UsingGpsTime);

	sleep(1);

	for (i = 0; i < 10; ++i) {
		frame[8] = i + 20;
		OpenSky::output_message(frame, MessageType_ModeSLong);
	}

	sleep(5);

	OpenSky::disable();

	sleep(2);
}
