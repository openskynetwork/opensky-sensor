#ifndef _HAVE_STATISTICS_H
#define _HAVE_STATISTICS_H

#include <stdint.h>

struct STAT_Statistics {
	uint64_t ADSB_outOfSync;
	uint64_t ADSB_frameType[4];
	uint64_t ADSB_frameTypeUnknown;
	uint64_t ADSB_framesFiltered;
	uint64_t ADSB_framesUnsynchronized;

	uint64_t BUF_flushes;
	uint64_t BUF_GCRuns;
	uint64_t BUF_uncollects;
	uint64_t BUF_pool;
	uint64_t BUF_pools;
	uint64_t BUF_poolsCreated;
	uint64_t BUF_maxPools;
	uint64_t BUF_sacrifices;
	uint64_t BUF_maxQueue;
	uint64_t BUF_queue;
	uint64_t BUF_sacrificeMax;

	uint64_t WD_events;

	uint64_t NET_framesFailed;
	uint64_t NET_framesSent;
	uint64_t NET_connectsFail;
	uint64_t NET_connectsSuccess;
	uint64_t NET_keepAlives;
};

extern struct STAT_Statistics STAT_stats;

void STAT_init(uint32_t interval);
void STAT_main();

#endif
