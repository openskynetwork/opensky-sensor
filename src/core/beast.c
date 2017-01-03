/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <string.h>
#include "beast.h"
#include "util/util.h"

/** Escape all SYNC characters in a packet to comply with the beast frame
 *   format.
 * @param out output buffer, must be large enough (twice the length)
 * @param in input buffer
 * @param len input buffer length
 * @return used output buffer length
 * @note input and output buffers may not overlap
 */
size_t BEAST_encode(uint8_t * __restrict out, const uint8_t * __restrict in,
	size_t len)
{
	if (unlikely(!len))
		return 0;

	/* general idea is to search for a SYNC, copy the buffer up to (including)
	 * the sync and then point to SYNC and search for the next SYNC.
	 * In that second copy, we copy the first SYNC _again_ to the output buffer,
	 * thereby doubling it.
	 * Besides dealing with a regular end of buffer, we also have to deal with
	 * special cases such as the beginning, the last SYNC or if there is no
	 * SYNC at all.
	 */

	uint8_t * ptr = out;
	const uint8_t * end = in + len;

	/* first time: search for SYNC from input buffer up to len */
	const uint8_t * sync = (uint8_t*)memchr(in, BEAST_SYNC, len);
	while (true) {
		if (unlikely(sync)) {
			/* if SYNC found: copy up to (including) SYNC */
			memcpy(ptr, in, sync + 1 - in);
			ptr += sync + 1 - in;
			in = sync; /* important: in points to that SYNC now */
		} else {
			/* no (more) SYNC found: copy rest, fast return */
			memcpy(ptr, in, end - in);
			return ptr + (end - in) - out;
		}
		if (likely(end != in + 1)) {
			/* there is some input left -> search for next SYNC, this time
			 * beginning _after_ the current SYNC
			 */
			sync = (uint8_t*)memchr(in + 1, BEAST_SYNC, end - in - 1);
		} else {
			/* nothing more to do, but the last SYNC is still to be copied ->
			 * instead of setting sync = NULL and re-iterating, we use a little
			 * shortcut here.
			 */
			*ptr = BEAST_SYNC;
			return ptr + 1 - out;
		}
	}
}
