/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <opensky.hh>
#include <unistd.h>
#include <cstring>
#include <endian.h>
#include <stdint.h>

int main()
{
	OpenSky::configure();

	OpenSky::enable();

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	unsigned char frame[6 + 1 + 14];
	::uint64_t mlat = ::htobe64(123456789012345);
	::memcpy(frame, &mlat, 6);
	frame[6] = 100;
	frame[7] = 0x8a;

	uint32_t i = 0;
	for (i = 0; i < 10; ++i) {
		frame[8] = i;
		OpenSky::output_message(frame, MessageType_ModeSLong);
	}

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	OpenSky::disable();
}
