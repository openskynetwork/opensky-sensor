/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "filter.h"
#include "input.h"
#include "util/statistics.h"
#include "util/cfgfile.h"
#include "util/util.h"

/** Mode-S frame types, as bitmask for filtering*/
enum MODES_TYPE {
	/** Let no frames pass */
	MODES_TYPE_NONE = 0,

	/** Let only extended squitter (type 17) pass */
	MODES_TYPE_EXTENDED_SQUITTER = 1 << 17,
	/** Let only extended squitter/no transponder (type 18) pass */
	MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	/** Let all frames pass */
	MODES_TYPE_ALL = ~0,

	/** Let all extended quitter pass */
	MODES_TYPE_EXTENDED_SQUITTER_ALL =
		MODES_TYPE_EXTENDED_SQUITTER |
		MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** Frame filter (for Mode-S frames) */
static enum MODES_TYPE modeSFilter;

/** Synchronization info: true if receiver has a valid timestamp */
static bool isSynchronized;

/** Filter configuration, exposed for input components */
struct FILTER_Configuration FILTER_cfg;

/** Statistics */
static struct FILTER_Statistics stats;

/** Configuration: Descriptor */
static const struct CFG_Section cfgDesc = {
	.name = "FILTER",
	.n_opt = 3,
	.options = {
		{
			.name = "CRC",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &FILTER_cfg.crc,
			.def = { .boolean = true }
		},
		{
			.name = "ModeSExtSquitterOnly",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &FILTER_cfg.extSquitter,
			.def = { .boolean = true }
		},
		{
			.name = "SyncFilter",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &FILTER_cfg.sync,
			.def = { .boolean = true }
		}
	}
};

/** synchronization info: true if receiver has a valid GPS timestamp */
static bool isSynchronized;

static bool construct();
static void resetStats();

const struct Component FILTER_comp = {
	.description = "FILTER",
	.config = &cfgDesc,
	.onConstruct = &construct,
	.onReset = &resetStats,
	.dependencies = { NULL }
};

/** Component: Initialize filter.
 * @return always true
 */
static bool construct()
{
	modeSFilter = FILTER_cfg.extSquitter ?
		MODES_TYPE_EXTENDED_SQUITTER_ALL : MODES_TYPE_ALL;
	FILTER_reset();

	return true;
}

/** Statistics: reset all statistics */
static void resetStats()
{
	memset(&stats, 0, sizeof stats);
}

void FILTER_getStatistics(struct FILTER_Statistics * statistics)
{
	memcpy(statistics, &stats, sizeof *statistics);
}

/** Reset the filter state */
void FILTER_reset()
{
	isSynchronized = false;
}

/** Set filter configuration: set synchronization filter
 * @param syncFilter enable synchronization filter
 */
void FILTER_setSynchronizedFilter(bool syncFilter)
{
	FILTER_cfg.sync = syncFilter;
}

/** Set filert configuration: set Mode-S extended squitter only filter
 * @param modeSExtSquitter let only Mode-S extended squitter frames pass
 */
void FILTER_setModeSExtSquitter(bool modeSExtSquitter)
{
	FILTER_cfg.extSquitter = modeSExtSquitter;
	modeSFilter = FILTER_cfg.extSquitter ?
		MODES_TYPE_EXTENDED_SQUITTER_ALL : MODES_TYPE_ALL;
	/* cross layer: this can have an impact on the input */
	INPUT_reconfigure();
}

/** Set filter state
 * @param synchronized set synchronization state
 */
void FILTER_setSynchronized(bool synchronized)
{
	isSynchronized = synchronized;
}

/** Filter: test frame
 * @param frameType frame type
 * @param firstByte first byte of frame
 * @return true if frame can pass, false if it should be filtered
 */
bool FILTER_filter(enum OPENSKY_FRAME_TYPE frameType, uint8_t firstByte)
{
	++stats.framesByType[frameType];

	/* apply synchronization filter */
	if (unlikely(!isSynchronized)) {
		++stats.unsynchronized;
		if (FILTER_cfg.sync) {
			++stats.filtered;
			return false;
		}
	}

	/* apply frame filter */
	if (frameType != OPENSKY_FRAME_TYPE_MODE_S_LONG &&
		frameType != OPENSKY_FRAME_TYPE_MODE_S_SHORT) {
		++stats.filtered;
		return false;
	}

	uint_fast32_t ftype = (firstByte >> 3) & 0x1f;
	++stats.modeSByType[ftype];
	/* apply filter */
	if (!((1 << ftype) & modeSFilter)) {
		++stats.filtered;
		++stats.modeSfiltered;
		return false;
	}

	return true;
}
