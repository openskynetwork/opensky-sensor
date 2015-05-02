
#include <statistics.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <buffer.h>
#include <signal.h>

volatile struct STAT_Statistics STAT_stats;

const const struct CFG_STATS * config;

static time_t start;
static char startstr[26];

static void sigStats(int sig);
static void printStatistics();

/** Initialize statistics.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 */
void STAT_init(const struct CFG_STATS * cfg)
{
	start = time(NULL);
	ctime_r(&start, startstr);

	memset((void*)&STAT_stats, 0, sizeof STAT_stats);

	config = cfg;

	signal(SIGUSR1, &sigStats);
}

void STAT_main()
{
	while (true) {
		sleep(config->interval);

		printStatistics();
	}
}

static void sigStats(int sig)
{
	printStatistics();
}

static void printStatistics()
{
	struct STAT_Statistics snapshot;

	time_t now = time(NULL);

	BUF_fillStatistics();
	memcpy(&snapshot, (void*)&STAT_stats, sizeof snapshot);

	uint32_t secs = now - start;
	uint32_t d = secs / (24 * 3600);
	uint32_t h = (secs / 3600) % 24;
	uint32_t m = (secs / 60) % 60;
	uint32_t s = secs % 60;

	puts("Statistics");
	printf(" - started on %s", startstr);
	printf(" - running since %3" PRIu32 "d, %02" PRIu32 "h:%02" PRIu32
		"m:%02" PRIu32 "s\n", d, h, m, s);
	puts("");

	puts(" Watchdog");
	printf(" - %27" PRIu64 " events\n", snapshot.WD_events);
	puts("");

	puts(" ADSB");
	printf(" - %27" PRIu64 " times out of sync\n", snapshot.ADSB_outOfSync);
	uint64_t frames = snapshot.ADSB_frameType[0] +
		snapshot.ADSB_frameType[1] + snapshot.ADSB_frameType[2] +
		snapshot.ADSB_frameTypeUnknown + snapshot.ADSB_framesFiltered;
	printf(" - %27" PRIu64 " frames received (%.02f per second)\n",
		frames, (double)frames / secs);
	printf(" - %27" PRIu64 " Mode-A/C frames (%.02f per second)\n",
		snapshot.ADSB_frameType[0],
		(double)snapshot.ADSB_frameType[0] / secs);
	printf(" - %27" PRIu64 " Mode-S Short frames (%.02f per second)\n",
		snapshot.ADSB_frameType[1],
		(double)snapshot.ADSB_frameType[1] / secs);
	printf(" - %27" PRIu64 " Mode-S Long frames (%.02f per second)\n",
		snapshot.ADSB_frameType[2],
		(double)snapshot.ADSB_frameType[2] / secs);
	uint32_t i;
	for (i = 0; i < 32; ++i) {
		uint64_t n = snapshot.ADSB_longType[i];
		if (n)
			printf("   - %27" PRIu64 " frames of type %" PRIu32
				" (%.02f /s)\n", n, i, (double)n / secs);
	}
	printf(" - %27" PRIu64 " status frames (%.02f per second)\n",
		snapshot.ADSB_frameType[3],
		(double)snapshot.ADSB_frameType[3] / secs);
	printf(" - %27" PRIu64 " unknown frames (%.02f per second)\n",
		snapshot.ADSB_frameTypeUnknown,
		(double)snapshot.ADSB_frameTypeUnknown / secs);
	printf(" - %27" PRIu64 " frames filtered (%.02f per second)\n",
		snapshot.ADSB_framesFiltered,
		(double)snapshot.ADSB_framesFiltered / secs);
	printf(" - %27" PRIu64 " long frames filtered (%.02f per second)\n",
		snapshot.ADSB_framesFilteredLong,
		(double)snapshot.ADSB_framesFilteredLong / secs);
	printf(" - %27" PRIu64 " unsynchronized frames\n",
		snapshot.ADSB_framesUnsynchronized);
	puts("");

	puts(" Buffer");
	printf(" - %27" PRIu64 " queued messages (current)\n",
		snapshot.BUF_queue);
	printf(" - %27" PRIu64 " queued messages (max)\n",
		snapshot.BUF_maxQueue);
	printf(" - %27" PRIu64 " discarded messages (overall)\n",
		snapshot.BUF_sacrifices);
	printf(" - %27" PRIu64 " discarded messages (in one overflow "
		"situation)\n", snapshot.BUF_sacrificeMax);
	printf(" - %27" PRIu64 " frames in pool (Usage %.2f%%)\n",
		snapshot.BUF_pool, (100. * snapshot.BUF_queue) /
		(double)(snapshot.BUF_queue + snapshot.BUF_pool));
	printf(" - %27" PRIu64 " dynamic pools (current)\n",
		snapshot.BUF_pools);
	printf(" - %27" PRIu64 " dynamic pools (overall)\n",
		snapshot.BUF_poolsCreated);
	printf(" - %27" PRIu64 " dynamic pools (max)\n",
		snapshot.BUF_maxPools);
	printf(" - %27" PRIu64 " flushes\n", snapshot.BUF_flushes);
	printf(" - %27" PRIu64 " uncollected dynamic pools\n",
		snapshot.BUF_uncollects);
	printf(" - %27" PRIu64 " Garbage Collector runs\n",
		snapshot.BUF_GCRuns);
	puts("");

	puts(" Network");
	printf(" - %27" PRIu64 " successful connection attempts\n",
		snapshot.NET_connectsSuccess);
	printf(" - %27" PRIu64 " unsuccessful connection attempts\n",
		snapshot.NET_connectsFail);
	printf(" - %27" PRIu64 " frames sent successfully\n",
		snapshot.NET_framesSent);
	printf(" - %27" PRIu64 " frames sent unsuccessfully\n",
		snapshot.NET_framesFailed);
	printf(" - %27" PRIu64 " keep alive messages\n",
		snapshot.NET_keepAlives);
}
