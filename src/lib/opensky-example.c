/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <opensky.h>
#include <unistd.h>

int main()
{
	OPENSKY_configure();

	OPENSKY_start();

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	const struct OPENSKY_Frame frameSync = {
		.frameType = OPENSKY_FRAME_TYPE_STATUS,
		.mlat = 1
	};

	OPENSKY_frame(&frameSync);

	struct OPENSKY_Frame frame = {
		.frameType = OPENSKY_FRAME_TYPE_MODE_S_LONG,
		.siglevel = 100,
		.mlat = 123456789012345,
		.payload = { 0x8a },
		.payloadLen = 14
	};

	uint32_t i = 0;
	for (i = 0; i < 10; ++i) {
		frame.payload[1] = i;
		OPENSKY_frame(&frame);
	}

	/* sleep is normally not needed, just for some testing purpose */
	sleep(1);

	OPENSKY_stop();
}
