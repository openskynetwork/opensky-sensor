/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#include <filter.h>
#include <statistics.h>
#include <cfgfile.h>
#include <util.h>

enum MODES_TYPE {
	MODES_TYPE_NONE = 0,

	MODES_TYPE_EXTENDED_SQUITTER = 1 << 17,
	MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	MODES_TYPE_ALL = ~0,
	MODES__TYPE_EXTENDED_SQUITTER_ALL =
		MODES_TYPE_EXTENDED_SQUITTER |
		MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** frame filter (for Mode-S frames) */
static enum MODES_TYPE modeSFilter;

/** synchronization info: true if receiver has a valid GPS timestamp */
static bool isSynchronized;

void FILTER_init()
{
	modeSFilter = CFG_config.recv.modeSLongExtSquitter ?
		MODES__TYPE_EXTENDED_SQUITTER_ALL : MODES_TYPE_ALL;
	isSynchronized = false;
}

void FILTER_reset()
{
	isSynchronized = false;
}

void FILTER_setSynchronized(bool synchronized)
{
	isSynchronized = synchronized;
}

bool FILTER_filter(enum ADSB_FRAME_TYPE frameType, uint8_t firstByte)
{
	++STAT_stats.RECV_frameType[frameType];

	if (unlikely(!isSynchronized)) {
		++STAT_stats.RECV_framesUnsynchronized;
		if (CFG_config.recv.syncFilter) {
			++STAT_stats.RECV_framesFiltered;
			return false;
		}
	}

	/* apply filter */
	if (frameType != ADSB_FRAME_TYPE_MODE_S_LONG) {
		++STAT_stats.RECV_framesFiltered;
		return false;
	}

	uint_fast32_t ftype = (firstByte >> 3) & 0x1f;
	++STAT_stats.RECV_modeSType[ftype];
	/* apply filter */
	if (!((1 << ftype) & modeSFilter)) {
		++STAT_stats.RECV_framesFiltered;
		++STAT_stats.RECV_modeSFilteredLong;
		return false;
	}

	return true;
}
