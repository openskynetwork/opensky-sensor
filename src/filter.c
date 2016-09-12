/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include "filter.h"
#include "statistics.h"
#include "cfgfile.h"
#include "util.h"
#include "input.h"

enum MODES_TYPE {
	MODES_TYPE_NONE = 0,

	MODES_TYPE_EXTENDED_SQUITTER = 1 << 17,
	MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	MODES_TYPE_ALL = ~0,
	MODES_TYPE_EXTENDED_SQUITTER_ALL =
		MODES_TYPE_EXTENDED_SQUITTER |
		MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** frame filter (for Mode-S frames) */
static enum MODES_TYPE modeSFilter;

struct FILTER_Configuration FILTER_cfg;

static const struct CFG_Section cfg = {
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

const struct Component FILTER_comp = {
	.description = "FILTER",
	.config = &cfg,
	.dependencies = { NULL }
};

void FILTER_init()
{
	modeSFilter = FILTER_cfg.extSquitter ?
		MODES_TYPE_EXTENDED_SQUITTER_ALL : MODES_TYPE_ALL;
	FILTER_reset();
}

void FILTER_reset()
{
	isSynchronized = false;
}

void FILTER_setModeSExtSquitter(bool modeSExtSquitter)
{
	FILTER_cfg.extSquitter = modeSExtSquitter;
	modeSFilter = FILTER_cfg.extSquitter ?
		MODES_TYPE_EXTENDED_SQUITTER_ALL : MODES_TYPE_ALL;
	INPUT_reconfigure();
}

void FILTER_setSynchronized(bool synchronized)
{
	isSynchronized = synchronized;
}

bool FILTER_filter(enum OPENSKY_FRAME_TYPE frameType, uint8_t firstByte)
{
	++STAT_stats.RECV_frameType[frameType];

	if (unlikely(!isSynchronized)) {
		++STAT_stats.RECV_framesUnsynchronized;
		if (FILTER_cfg.sync) {
			++STAT_stats.RECV_framesFiltered;
			return false;
		}
	}

	/* apply filter */
	if (frameType != OPENSKY_FRAME_TYPE_MODE_S_LONG &&
		frameType != OPENSKY_FRAME_TYPE_MODE_S_SHORT) {
		++STAT_stats.RECV_framesFiltered;
		return false;
	}

	uint_fast32_t ftype = (firstByte >> 3) & 0x1f;
	++STAT_stats.RECV_modeSType[ftype];
	/* apply filter */
	if (!((1 << ftype) & modeSFilter)) {
		++STAT_stats.RECV_framesFiltered;
		++STAT_stats.RECV_modeSFiltered;
		return false;
	}

	return true;
}
