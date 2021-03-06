/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_BUFFER_H
#define _HAVE_BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include "util/component.h"
#include "openskytypes.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BUFFER_Statistics {
	uint64_t queueSize;
	uint64_t maxQueueSize;
	uint64_t discardedCur;
	uint64_t discardedAll;
	uint64_t discardedMax;
	uint64_t poolSize;
	uint64_t dynPools;
	uint64_t dynPoolsAll;
	uint64_t dynPoolsMax;
	uint64_t uncollectedPools;
	uint64_t flushes;
	uint64_t GCruns;
};

extern const struct Component BUF_comp;

void BUF_flush();
void BUF_flushUnlessHistoryEnabled();

void BUF_runGC();

struct OPENSKY_RawFrame * BUF_newFrame();
void BUF_commitFrame(struct OPENSKY_RawFrame * frame);
void BUF_abortFrame(struct OPENSKY_RawFrame * frame);

const struct OPENSKY_RawFrame * BUF_getFrame();
const struct OPENSKY_RawFrame * BUF_getFrameTimeout(uint_fast32_t timeout_ms);
void BUF_releaseFrame(const struct OPENSKY_RawFrame * frame);
void BUF_putFrame(const struct OPENSKY_RawFrame * frame);

void BUFFER_getStatistics(struct BUFFER_Statistics * statistics);

#ifdef __cplusplus
}
#endif

#endif
