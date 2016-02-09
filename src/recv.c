/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <recv.h>
#include <message.h>
#include <adsb.h>
#include <buffer.h>
#include <threads.h>
#include <util.h>
#include <filter.h>

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

	FILTER_init();
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

		FILTER_reset();

		frame = BUF_newFrame();
		while (true) {
			bool success = ADSB_getFrame(frame);
			if (likely(success)) {
				if (FILTER_filter(frame)) {
					BUF_commitFrame(frame);
					frame = BUF_newFrame();
				}
			} else {
				BUF_abortFrame(frame);
				frame = NULL;
				break;
			}
		}
	}
	CLEANUP_POP();
}
