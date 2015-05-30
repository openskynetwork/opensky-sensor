#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <recv.h>
#include <adsb.h>
#include <buffer.h>
#include <statistics.h>
#include <cfgfile.h>
#include <threads.h>

enum RECV_LONG_FRAME_TYPE {
	RECV_LONG_FRAME_TYPE_NONE = 0,

	RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER = 1 << 17,
	RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	RECV_LONG_FRAME_TYPE_ALL = ~0,
	RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL =
		RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER |
		RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** frame filter (for long frames) */
static enum RECV_LONG_FRAME_TYPE frameFilterLong;

/** synchronization info: true if receiver has a valid GPS timestamp */
static bool isSynchronized;

static void construct();
static void destruct();
static void mainloop();

struct Component RECV_comp = {
	.description = "RECV",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop
};

static void construct()
{
#ifndef NETWORK
	struct ADSB_CONFIG adsb_config;
	adsb_config.crc = CFG_config.recv.crc;
	adsb_config.fec = CFG_config.recv.fec;
	adsb_config.frameFilter = true;
	adsb_config.modeAC = false;
	adsb_config.rtscts = CFG_config.input.rtscts;
	adsb_config.timestampGPS = CFG_config.recv.gps;
	ADSB_init(&adsb_config);
#else
	ADSB_init(NULL);
#endif

	frameFilterLong = CFG_config.recv.modeSLongExtSquitter ?
		RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL : RECV_LONG_FRAME_TYPE_ALL;
}

static void destruct()
{
	ADSB_destruct();
}

static void cleanup(struct ADSB_Frame * frame)
{
	if (frame)
		BUF_abortMessage(frame);
}

static void mainloop()
{
	struct ADSB_Frame * frame = NULL;

	CLEANUP_PUSH(&cleanup, frame);
	while (RECV_comp.run) {
		ADSB_connect();
		isSynchronized = false;

		frame = BUF_newMessage();
		while (RECV_comp.run) {
			bool success = ADSB_getFrame(frame);
			if (success) {
				++STAT_stats.ADSB_frameType[frame->frameType];

				if (frame->frameType == ADSB_FRAME_TYPE_STATUS) {
					isSynchronized = frame->mlat != 0;
					continue;
				}

				/* filter if unsynchronized and filter is enabled */
				if (!isSynchronized) {
					++STAT_stats.ADSB_framesUnsynchronized;
					if (CFG_config.recv.syncFilter) {
						++STAT_stats.ADSB_framesFiltered;
						continue;
					}
				}

				/* apply filter */
				if (frame->frameType != ADSB_FRAME_TYPE_MODE_S_LONG) {
					++STAT_stats.ADSB_framesFiltered;
					continue;
				}

				uint32_t ftype = (frame->payload[0] >> 3) & 0x1f;
				++STAT_stats.ADSB_longType[ftype];
				/* apply filter */
				if (!((1 << ftype) & frameFilterLong)) {
					++STAT_stats.ADSB_framesFiltered;
					++STAT_stats.ADSB_framesFilteredLong;
					continue;
				}

				BUF_commitMessage(frame);
				frame = BUF_newMessage();
			} else {
				BUF_abortMessage(frame);
				break;
			}
		}
	}
	CLEANUP_POP();
}
