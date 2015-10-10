/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <recv.h>
#include <message.h>
#include <adsb.h>
#include <buffer.h>
#include <statistics.h>
#include <cfgfile.h>
#include <threads.h>
#include <util.h>

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
	ADSB_init();

	frameFilterLong = CFG_config.recv.modeSLongExtSquitter ?
		RECV_LONG_FRAME_TYPE_EXTENDED_SQUITTER_ALL : RECV_LONG_FRAME_TYPE_ALL;
}

static void destruct()
{
	ADSB_destruct();
}

static void cleanup(struct ADSB_Frame ** frame)
{
	if (*frame)
		BUF_abortFrame(*frame);
}

static void mainloop()
{
	struct ADSB_Frame * frame = NULL;

	CLEANUP_PUSH(&cleanup, &frame);
	while (true) {
		ADSB_connect();
		isSynchronized = false;

		frame = BUF_newFrame();
		while (true) {
			bool success = ADSB_getFrame(frame);
			if (likely(success)) {
				++STAT_stats.ADSB_frameType[frame->frameType];

				if (unlikely(frame->frameType == ADSB_FRAME_TYPE_STATUS)) {
					if (!isSynchronized)
						isSynchronized = frame->mlat != 0;
					continue;
				}

				/* filter if unsynchronized and filter is enabled */
				if (unlikely(!isSynchronized)) {
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

				BUF_commitFrame(frame);
				frame = BUF_newFrame();
			} else {
				BUF_abortFrame(frame);
				frame = NULL;
				break;
			}
		}
	}
	CLEANUP_POP();
}
