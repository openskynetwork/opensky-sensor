/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#include <filter.h>
#include <statistics.h>
#include <cfgfile.h>
#include <util.h>

enum LONG_FRAME_TYPE {
	LONG_FRAME_TYPE_NONE = 0,

	LONG_FRAME_TYPE_EXTENDED_SQUITTER = 1 << 17,
	LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	LONG_FRAME_TYPE_ALL = ~0,
	LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL =
		LONG_FRAME_TYPE_EXTENDED_SQUITTER |
		LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** frame filter (for long frames) */
static enum LONG_FRAME_TYPE frameFilterLong;

/** synchronization info: true if receiver has a valid GPS timestamp */
static bool isSynchronized;

void FILTER_init()
{
	frameFilterLong = CFG_config.recv.modeSLongExtSquitter ?
		LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL : LONG_FRAME_TYPE_ALL;
	isSynchronized = false;
}

void FILTER_reset()
{
	isSynchronized = false;
}

bool FILTER_filter(const struct ADSB_Frame * frame)
{
	++STAT_stats.ADSB_frameType[frame->frameType];

	if (unlikely(frame->frameType == ADSB_FRAME_TYPE_STATUS)) {
		if (!isSynchronized)
			isSynchronized = frame->mlat != 0;
		return false;
	}

	/* filter if unsynchronized and filter is enabled */
	if (unlikely(!isSynchronized)) {
		++STAT_stats.ADSB_framesUnsynchronized;
		if (CFG_config.recv.syncFilter) {
			++STAT_stats.ADSB_framesFiltered;
			return false;
		}
	}

	/* apply filter */
	if (frame->frameType != ADSB_FRAME_TYPE_MODE_S_LONG) {
		++STAT_stats.ADSB_framesFiltered;
		return false;
	}

	uint_fast32_t ftype = (frame->payload[0] >> 3) & 0x1f;
	++STAT_stats.ADSB_longType[ftype];
	/* apply filter */
	if (!((1 << ftype) & frameFilterLong)) {
		++STAT_stats.ADSB_framesFiltered;
		++STAT_stats.ADSB_framesFilteredLong;
		return false;
	}

	return true;
}
