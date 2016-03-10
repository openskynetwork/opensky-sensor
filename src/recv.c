/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

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

enum RECV_MODES_TYPE {
	RECV_MODES_TYPE_NONE = 0,

	RECV_MODES_TYPE_EXTENDED_SQUITTER = 1 << 17,
	RECV_MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER = 1 << 18,

	RECV_MODES_TYPE_ALL = ~0,
	RECV_MODES_TYPE_EXTENDED_SQUITTER_ALL =
		RECV_MODES_TYPE_EXTENDED_SQUITTER |
		RECV_MODES_TYPE_EXTENDED_SQUITTER_NON_TRANSPONDER
};

/** frame filter (for Mode-S frames) */
static enum RECV_MODES_TYPE modeSFilter;

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

	modeSFilter = CFG_config.recv.modeSLongExtSquitter ?
		RECV_MODES_TYPE_EXTENDED_SQUITTER_ALL : RECV_MODES_TYPE_ALL;
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
				++STAT_stats.RECV_frameType[frame->frameType];

				if (unlikely(frame->frameType == ADSB_FRAME_TYPE_STATUS)) {
					if (!isSynchronized)
						isSynchronized = frame->mlat != 0;
					continue;
				}

				/* filter if unsynchronized and filter is enabled */
				if (unlikely(!isSynchronized)) {
					++STAT_stats.RECV_framesUnsynchronized;
					if (CFG_config.recv.syncFilter) {
						++STAT_stats.RECV_framesFiltered;
						continue;
					}
				}

				/* apply filter */
				if (frame->frameType != ADSB_FRAME_TYPE_MODE_S_LONG ||
					frame->frameType != ADSB_FRAME_TYPE_MODE_S_SHORT) {
					++STAT_stats.RECV_framesFiltered;
					continue;
				}

				uint_fast32_t ftype = (frame->payload[0] >> 3) & 0x1f;
				++STAT_stats.RECV_modeSType[ftype];
				/* apply filter */
				if (!((1 << ftype) & modeSFilter)) {
					++STAT_stats.RECV_framesFiltered;
					++STAT_stats.RECV_modeSFilteredLong;
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
