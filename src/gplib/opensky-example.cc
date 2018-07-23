/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#include <cstring>
#include <unistd.h>
#include <endian.h>
#include <stdint.h>

#include "opensky.hh"

int main()
{
	OpenSky::init();

	OpenSky::configure(OPENSKY_DEVICE_TYPE_BOGUS, 1234);

	OpenSky::enable();

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	std::uint8_t frame[14];
	frame[0] = 0x8a;

	std::uint32_t i;
	for (i = 0; i < 10; ++i) {
		frame[1] = i;
		OpenSky::submitFrame(i, true, true, -10, frame, sizeof frame);
	}

	sleep(1);
	OpenSky::setGpsPosition(4.3, 140.5, 250.);

	sleep(3);

	for (i = 0; i < 10; ++i) {
		frame[1] = i + 10;
		OpenSky::submitFrame(i, true, true, -20, frame, sizeof frame);
	}

	/* this frame should be filtered because of timing flags */
	OpenSky::submitFrame(1, false, false, -30, frame, sizeof frame);

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	OpenSky::disable();

	sleep(2);
}
