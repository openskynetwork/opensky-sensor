#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <statistics.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <buffer.h>
#include <signal.h>
#include <cfgfile.h>
#include <threads.h>

volatile struct STAT_Statistics STAT_stats;

struct Snapshot {
	time_t timestamp;
	uint32_t secs;
	uint32_t delta;
	struct STAT_Statistics stats;
	uint64_t frames;
};

static time_t start;
static char startstr[26];

static void construct();
static void destruct();
static void mainloop();

struct Component STAT_comp = {
	.description = "STAT",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop
};

static void sigStats(int sig);
static void printStatistics(struct Snapshot * lastSnapshot);

/** Initialize statistics.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 */
static void construct()
{
	start = time(NULL);
	ctime_r(&start, startstr);

	memset((void*)&STAT_stats, 0, sizeof STAT_stats);

	signal(SIGUSR1, &sigStats);
}

static void destruct()
{
	signal(SIGUSR1, SIG_DFL);
}

static void mainloop()
{
	struct Snapshot snapshot;

	memset(&snapshot, 0, sizeof snapshot);
	snapshot.timestamp = time(NULL);

	while (true) {
		sleep(CFG_config.stats.interval);

		__attribute__((unused)) int r;
		CANCEL_DISABLE(&r);
		printStatistics(&snapshot);
		CANCEL_RESTORE(&r);
	}
}

static void sigStats(int sig)
{
	printStatistics(NULL);
}

static inline void printFrames2(const char * name,
	const struct Snapshot * snapshot, uint64_t frames,
	bool hasLastFrames, uint64_t lastFrames)
{
	printf(" - %27" PRIu64 " [%3.0f%%] %s (%.02f /s", frames,
		100. * frames / snapshot->frames, name,
		(double)frames / snapshot->secs);
	if (hasLastFrames && snapshot->delta) {
		printf(", %.02f /s)\n",
			(double)(frames - lastFrames) / snapshot->delta);
	} else {
		puts(")");
	}
}

#define printFrames(name, snapshot, lastSnapshot, member) \
	printFrames2((name), (snapshot), (snapshot)->stats.member, \
		!!(lastSnapshot), (lastSnapshot) ? (lastSnapshot)->stats.member : 0)

static inline void printSubFrames(uint32_t type,
	const struct Snapshot * snapshot, const struct Snapshot * lastSnapshot)
{
	uint64_t n = snapshot->stats.ADSB_longType[type];
	if (n) {
		printf("   - %27" PRIu64 " [%3.0f%%] type %" PRIu32 " (%.02f /s",
			n, 100. * snapshot->stats.ADSB_frameType[2] / n, type,
			(double)n / snapshot->secs);
		if (lastSnapshot && snapshot->delta) {
			printf(", %.02f /s)\n",
				(double)(n - lastSnapshot->stats.ADSB_longType[type]) /
					snapshot->delta);
		} else {
			puts(")");
		}
	}
}

static void printStatistics(struct Snapshot * lastSnapshot)
{
	struct Snapshot snapshot;
	struct STAT_Statistics * stats = &snapshot.stats;

	snapshot.timestamp = time(NULL);

	snapshot.delta = lastSnapshot ?
		snapshot.timestamp - lastSnapshot->timestamp : 0;

	BUF_fillStatistics();
	memcpy(&snapshot.stats, (void*)&STAT_stats, sizeof snapshot.stats);

	snapshot.secs = snapshot.timestamp - start;
	uint32_t d = snapshot.secs / (24 * 3600);
	uint32_t h = (snapshot.secs / 3600) % 24;
	uint32_t m = (snapshot.secs / 60) % 60;
	uint32_t s = snapshot.secs % 60;

	puts("Statistics");
	printf(" - started on %s", startstr);
	printf(" - running since %3" PRIu32 "d, %02" PRIu32 "h:%02" PRIu32
		"m:%02" PRIu32 "s\n", d, h, m, s);
	puts("");

	puts(" Watchdog");
	printf(" - %27" PRIu64 " events\n", stats->WD_events);
	puts("");

	puts(" ADSB");
	printf(" - %27" PRIu64 " times out of sync\n", stats->ADSB_outOfSync);
	snapshot.frames = stats->ADSB_frameType[0] + stats->ADSB_frameType[1] +
		stats->ADSB_frameType[2] + stats->ADSB_frameTypeUnknown;
	printf(" - %27" PRIu64 " frames received (%.02f /s",
		snapshot.frames, (double)snapshot.frames / snapshot.secs);
	if (lastSnapshot) {
		printf(", %.02f /s)\n",
			(double)(snapshot.frames - lastSnapshot->frames) / snapshot.delta);
	} else {
		puts(")");
	}
	printFrames("Mode-A/C", &snapshot, lastSnapshot, ADSB_frameType[0]);
	printFrames("Mode-S Short", &snapshot, lastSnapshot, ADSB_frameType[1]);
	printFrames("Mode-S Long", &snapshot, lastSnapshot, ADSB_frameType[2]);
	uint32_t i;
	for (i = 0; i < 32; ++i)
		printSubFrames(i, &snapshot, lastSnapshot);
	printFrames("status", &snapshot, lastSnapshot, ADSB_frameType[3]);
	printFrames("unknown", &snapshot, lastSnapshot, ADSB_frameTypeUnknown);
	printFrames("filtered", &snapshot, lastSnapshot, ADSB_framesFiltered);
	printFrames("long filtered", &snapshot, lastSnapshot,
		ADSB_framesFilteredLong);
	printf(" - %27" PRIu64 " unsynchronized frames\n",
		stats->ADSB_framesUnsynchronized);
	puts("");

	puts(" Buffer");
	printf(" - %27" PRIu64 " queued messages (current)\n",
		stats->BUF_queue);
	printf(" - %27" PRIu64 " queued messages (max)\n",
		stats->BUF_maxQueue);
	printf(" - %27" PRIu64 " discarded messages (overall)\n",
		stats->BUF_sacrifices);
	printf(" - %27" PRIu64 " discarded messages (in one overflow "
		"situation)\n", stats->BUF_sacrificeMax);
	printf(" - %27" PRIu64 " frames in pool (Usage %.2f%%)\n",
		stats->BUF_pool, (100. * stats->BUF_queue) /
		(double)(stats->BUF_queue + stats->BUF_pool));
	printf(" - %27" PRIu64 " dynamic pools (current)\n",
		stats->BUF_pools);
	printf(" - %27" PRIu64 " dynamic pools (overall)\n",
		stats->BUF_poolsCreated);
	printf(" - %27" PRIu64 " dynamic pools (max)\n",
		stats->BUF_maxPools);
	printf(" - %27" PRIu64 " flushes\n", stats->BUF_flushes);
	printf(" - %27" PRIu64 " uncollected dynamic pools\n",
		stats->BUF_uncollects);
	printf(" - %27" PRIu64 " Garbage Collector runs\n",
		stats->BUF_GCRuns);
	puts("");

	puts(" Network");
	printf(" - %27" PRIu64 " successful connection attempts\n",
		stats->NET_connectsSuccess);
	printf(" - %27" PRIu64 " unsuccessful connection attempts\n",
		stats->NET_connectsFail);
	printf(" - %27" PRIu64 " frames sent successfully\n",
		stats->NET_framesSent);
	printf(" - %27" PRIu64 " frames sent unsuccessfully\n",
		stats->NET_framesFailed);
	printf(" - %27" PRIu64 " frames received unsuccessfully\n",
		stats->NET_recvFailed);
	printf(" - %27" PRIu64 " keep alive messages\n",
		stats->NET_keepAlives);

	if (lastSnapshot)
		memcpy(lastSnapshot, &snapshot, sizeof snapshot);
}
