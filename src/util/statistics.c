/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include "statistics.h"
#include "cfgfile.h"
#include "threads.h"
#include "core/buffer.h"
#include "core/network.h"
#include "core/filter.h"

struct Snapshot {
	time_t timestamp;
	uint_fast32_t secs;
	uint_fast32_t delta;

	struct BUFFER_Statistics buffer;
	struct NET_Statistics net;
	struct FILTER_Statistics filter;
	uint64_t frames;
};

static time_t start;
static char startstr[26];

static bool cfgEnabled;
static uint32_t cfgInterval;

static const struct CFG_Section cfg = {
	.name = "STATISTICS",
	.n_opt = 2,
	.options = {
		{
			.name = "Enabled",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfgEnabled,
			.def = { .boolean = true }
		},
		{
			.name = "Interval",
			.type = CFG_VALUE_TYPE_INT,
			.var = &cfgInterval,
			.def = { .integer = 600 }
		}
	}
};

static bool construct();
static void mainloop();

const struct Component STAT_comp = {
	.description = "STAT",
	.start = &cfgEnabled,
	.onConstruct = &construct,
	.main = &mainloop,
	.config = &cfg,
	.dependencies = { NULL }
};

static void printStatistics(struct Snapshot * lastSnapshot);

/** Initialize statistics.
 * \return always true
 */
static bool construct()
{
	start = time(NULL);
	ctime_r(&start, startstr);

	return true;
}

static void mainloop()
{
	struct Snapshot snapshot;

	memset(&snapshot, 0, sizeof snapshot);
	snapshot.timestamp = time(NULL);

	while (true) {
		sleep(cfgInterval);

		__attribute__((unused)) int r;
		CANCEL_DISABLE(&r);
		printStatistics(&snapshot);
		CANCEL_RESTORE(&r);
	}
}

static inline void printFrames2(const char * name,
	const struct Snapshot * snapshot, uint64_t frames, uint64_t lastFrames)
{
	printf(" - %27" PRIu64 " [%3.0f%%] %s (%.02f /s", frames,
		snapshot->frames != 0 ? 100. * frames / snapshot->frames : 0., name,
		(double)frames / snapshot->secs);
	if (snapshot->delta) {
		printf(", %.02f /s)\n",
			(double)(frames - lastFrames) / snapshot->delta);
	} else {
		puts(")");
	}
}

#define printFrames(name, snapshot, lastSnapshot, member) \
	printFrames2((name), (snapshot), (snapshot)->filter.member, \
	(lastSnapshot)->filter.member)

static inline void printSubFrames(uint_fast32_t type,
	const struct Snapshot * snapshot, const struct Snapshot * lastSnapshot,
	uint64_t modeSFrames)
{
	uint64_t n = snapshot->filter.modeSByType[type];
	if (n) {
		printf("   - %27" PRIu64 " [%3.0f%%] type %" PRIuFAST32
			" (%.02f /s", n, 100. * n / modeSFrames, type,
			(double)n / snapshot->secs);
		if (snapshot->delta) {
			printf(", %.02f /s)\n",
				(double)(n - lastSnapshot->filter.modeSByType[type])
					/ snapshot->delta);
		} else {
			puts(")");
		}
	}
}

static void printStatistics(struct Snapshot * lastSnapshot)
{
	struct Snapshot snapshot;

	snapshot.timestamp = time(NULL);

	snapshot.delta = snapshot.timestamp - lastSnapshot->timestamp;

	BUFFER_getStatistics(&snapshot.buffer);
	NET_getStatistics(&snapshot.net);
	FILTER_getStatistics(&snapshot.filter);

	snapshot.secs = snapshot.timestamp - start;
	uint_fast32_t d = snapshot.secs / (24 * 3600);
	uint_fast32_t h = (snapshot.secs / 3600) % 24;
	uint_fast32_t m = (snapshot.secs / 60) % 60;
	uint_fast32_t s = snapshot.secs % 60;

	puts("Statistics");
	printf(" - started on %s", startstr);
	printf(" - running since %3" PRIuFAST32 "d, %02" PRIuFAST32
		"h:%02" PRIuFAST32 "m:%02" PRIuFAST32 "s\n", d, h, m, s);
	puts("");

	/*
	puts(" Receiver");
	printf(" - %27" PRIu64 " times out of sync\n", stats->RECV_outOfSync);
	snapshot.frames = stats->RECV_frameType[0] + stats->RECV_frameType[1]
		+ stats->RECV_frameType[2] + stats->RECV_frameTypeUnknown;
	*/

	snapshot.frames = snapshot.filter.framesByType[0] +
		snapshot.filter.framesByType[1] + snapshot.filter.framesByType[2] +
		snapshot.filter.framesByType[3] + snapshot.filter.unknown;
	puts(" Receiver");
	printf(" - %27" PRIu64 " frames received (%.02f /s",
			snapshot.frames, (double)snapshot.frames / snapshot.secs);
	if (snapshot.delta) {
		printf(", %.02f /s)\n",
			(double)(snapshot.frames - lastSnapshot->frames) / snapshot.delta);
	} else {
		puts(")");
	}
	printFrames("Mode-A/C", &snapshot, lastSnapshot, framesByType[0]);
	uint64_t modeSFrames = snapshot.filter.framesByType[1] +
		snapshot.filter.framesByType[2];
	printFrames("Mode-S Short", &snapshot, lastSnapshot, framesByType[1]);
	printFrames("Mode-S Long", &snapshot, lastSnapshot, framesByType[2]);
	uint_fast32_t i;
	for (i = 0; i < 32; ++i)
		printSubFrames(i, &snapshot, lastSnapshot, modeSFrames);
	printFrames("status", &snapshot, lastSnapshot, framesByType[3]);
	printFrames("unknown", &snapshot, lastSnapshot, unknown);
	printFrames("filtered", &snapshot, lastSnapshot, filtered);
	printFrames("Mode-S filtered", &snapshot, lastSnapshot,
		modeSfiltered);
	printf(" - %27" PRIu64 " unsynchronized frames\n",
		snapshot.filter.unsynchronized);
	puts("");

	const struct BUFFER_Statistics * buf = &snapshot.buffer;
	puts(" Buffer");
	if (buf->discardedCur != 0)
		puts(" -                             currently in overload");
	printf(" - %27" PRIu64 " queued messages (current)\n", buf->queueSize);
	printf(" - %27" PRIu64 " queued messages (max)\n", buf->maxQueueSize);
	printf(" - %27" PRIu64 " discarded messages (current)\n",
		buf->discardedCur);
	printf(" - %27" PRIu64 " discarded messages (overall)\n",
		buf->discardedAll);
	printf(" - %27" PRIu64 " discarded messages (in worst overflow "
		"situation)\n", buf->discardedMax);
	printf(" - %27" PRIu64 " messages in pool (Usage %.2f%%)\n",
		buf->poolSize,
		(100. * buf->queueSize)
			/ (double)(buf->poolSize + buf->queueSize));
	printf(" - %27" PRIu64 " dynamic pools (current)\n", buf->dynPools);
	printf(" - %27" PRIu64 " dynamic pools (overall)\n",
		buf->dynPoolsAll);
	printf(" - %27" PRIu64 " dynamic pools (max)\n", buf->dynPoolsMax);
	printf(" - %27" PRIu64 " flushes\n", buf->flushes);
	printf(" - %27" PRIu64 " uncollected dynamic pools\n",
		buf->uncollectedPools);
	printf(" - %27" PRIu64 " Garbage Collector runs\n", buf->GCruns);
	puts("");

	puts(" Network");
	const struct NET_Statistics * net = &snapshot.net;
	printf(" -                             currently %s\n", net->isOnline ?
		"online" : "offline");
	printf(" - %27" PRIu64 " [%3.0f%%] seconds online (overall)\n",
		net->onlineSecs, 100. * net->onlineSecs / snapshot.secs);
	printf(" - %27" PRIu64 " disconnections\n", net->disconnects);
	printf(" - %27" PRIu64 " connection attempts\n", net->connectionAttempts);
	printf(" - %27" PRIu64 " bytes sent (%.02f/s)\n", net->bytesSent,
		(double)net->bytesSent / snapshot.secs);
	printf(" - %27" PRIu64 " bytes received (%.02f/s)\n", net->bytesReceived,
		(double)net->bytesReceived / snapshot.secs);

	memcpy(lastSnapshot, &snapshot, sizeof snapshot);
}
