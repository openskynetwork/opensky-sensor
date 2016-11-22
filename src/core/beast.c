/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <string.h>
#include "beast.h"
#include "util/util.h"

/** Escape all SYNC characters in a packet to comply with the beast frame
 *   format.
 * \param out output buffer, must be large enough (twice the length)
 * \param in input buffer
 * \param len input buffer length
 * \note input and output buffers may not overlap
 */
size_t BEAST_encode(uint8_t * __restrict out, const uint8_t * __restrict in,
	size_t len)
{
	if (unlikely(!len))
		return 0;

	uint8_t * ptr = out;
	const uint8_t * end = in + len;

	/* first time: search for escape from in up to len */
	const uint8_t * esc = (uint8_t*) memchr(in, BEAST_SYNC, len);
	while (true) {
		if (unlikely(esc)) {
			/* if esc found: copy up to (including) esc */
			memcpy(ptr, in, esc + 1 - in);
			ptr += esc + 1 - in;
			in = esc; /* important: in points to the esc now */
		} else {
			/* no esc found: copy rest, fast return */
			memcpy(ptr, in, end - in);
			return ptr + (end - in) - out;
		}
		if (likely(end != in + 1)) {
			esc = (uint8_t*) memchr(in + 1, BEAST_SYNC, end - in - 1);
		} else {
			/* nothing more to do, but the last BEAST_SYNC is still to be copied
			 * instead of setting esc = NULL and re-iterating, we do things
			 * faster here.
			 */
			*ptr = BEAST_SYNC;
			return ptr + 1 - out;
		}
	}
}
