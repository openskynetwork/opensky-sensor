/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_STATISTICS_H
#define _HAVE_STATISTICS_H

#include <stdint.h>
#include "component.h"

#ifdef __cplusplus
extern "C" {
#endif

struct STAT_Statistics {
	uint_fast64_t RECV_outOfSync;
	uint_fast64_t RECV_frameType[4];
	uint_fast64_t RECV_modeSType[32];
	uint_fast64_t RECV_frameTypeUnknown;
	uint_fast64_t RECV_framesFiltered;
	uint_fast64_t RECV_modeSFiltered;
	uint_fast64_t RECV_framesUnsynchronized;

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

extern const struct Component STAT_comp;

extern volatile struct STAT_Statistics STAT_stats;

#ifdef __cplusplus
}
#endif

#endif
