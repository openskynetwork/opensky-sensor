/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <cstring>
#include <unistd.h>
#include <endian.h>
#include <stdint.h>
#include "opensky.hh"
#include "../cfgfile.h"
#include "../openskytypes.h"
#include "../input.h"

extern "C" {

void BUF_fillStatistics() {}

}

int main()
{
	CFG_loadDefaults();
	CFG_check();

	OpenSky::init();

	//OpenSky::configure();

	OpenSky::enable();

	INPUT_init();
	while (true) {
		INPUT_connect();

		struct OPENSKY_RawFrame rawFrame;
		struct OPENSKY_DecodedFrame frame;

		while (INPUT_getFrame(&rawFrame, &frame)) {
			unsigned char serialFrame[6 + 1 + 14];

			if (frame.frameType == OPENSKY_FRAME_TYPE_STATUS) {
				if (frame.mlat != 0)
					OpenSky::setGpsTimeStatus(UsingGpsTime);
				continue;
			}

			uint64_t mlat = htobe64(frame.mlat);
			memcpy(serialFrame, ((uint8_t*)&mlat) + 2, 6);
			serialFrame[6] = frame.siglevel;
			memcpy(serialFrame + 7, frame.payload, frame.payloadLen);

			OpenSky::output_message(serialFrame,
				(enum MessageType_T)(frame.frameType + MessageType_ModeAC));
		}
	}
}
