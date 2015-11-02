/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_STATISTICS_H
#define _HAVE_STATISTICS_H

#include <stdint.h>
#include <component.h>
#include <message.h>

struct STAT_Statistics {
	uint_fast64_t ADSB_outOfSync;
	uint_fast64_t ADSB_frameType[4];
	uint_fast64_t ADSB_longType[32];
	uint_fast64_t ADSB_frameTypeUnknown;
	uint_fast64_t ADSB_framesFiltered;
	uint_fast64_t ADSB_framesFilteredLong;
	uint_fast64_t ADSB_framesUnsynchronized;

	uint_fast64_t BUF_flushes;
	uint_fast64_t BUF_GCRuns;
	uint_fast64_t BUF_uncollects;
	uint_fast64_t BUF_pool;
	uint_fast64_t BUF_pools;
	uint_fast64_t BUF_poolsCreated;
	uint_fast64_t BUF_maxPools;
	uint_fast64_t BUF_sacrifices;
	uint_fast64_t BUF_overload;
	uint_fast64_t BUF_maxQueue;
	uint_fast64_t BUF_queue;
	uint_fast64_t BUF_sacrificeMax;

	uint_fast64_t WD_events;

	uint_fast64_t NET_msgsFailed;
	uint_fast64_t NET_msgsSent;
	uint_fast64_t NET_connectsFail;
	uint_fast64_t NET_connectsSuccess;
	uint_fast64_t NET_keepAlives;
	uint_fast64_t NET_msgsRecvFailed;
};

extern struct Component STAT_comp;

extern volatile struct STAT_Statistics STAT_stats;

#endif
