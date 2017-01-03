/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <unistd.h>
#include "trimble_input.h"

/** Register Trimble input */
void TRIMBLE_INPUT_register()
{
}

/** Initialize Trimble input */
void TRIMBLE_INPUT_init()
{
}

/** Disconnect Trimble input */
void TRIMBLE_INPUT_disconnect()
{
}

/** Reconnect Trimble input until success */
void TRIMBLE_INPUT_connect()
{
}

/** Read from Trimble receiver.
 * @param buf buffer to read into
 * @param bufLen buffer size
 * @return used buffer length or 0 on failure
 */
size_t TRIMBLE_INPUT_read(uint8_t * buf, size_t bufLen)
{
	while (true) {
		sleep(10);
	}
}

/** Write to Trimble receiver.
 * @param buf buffer to be written
 * @param bufLen length of buffer
 * @return number of bytes written or 0 on failure
 */
size_t TRIMBLE_INPUT_write(const uint8_t * buf, size_t bufLen)
{
	return bufLen;
}
