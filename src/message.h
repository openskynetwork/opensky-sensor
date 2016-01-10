/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_MESSAGE_H
#define _HAVE_MESSAGE_H

#include <adsb.h>

enum MSG_TYPE {
	MSG_TYPE_ADSB = 0,
	MSG_TYPE_LOG = 1,
	MSG_TYPE_STAT = 2,

	MSG_TYPES = 3,

	MSG_INVALID = 0xff
};

extern const char * MSG_TYPE_NAMES[MSG_TYPES];

struct MSG_Message {
	enum MSG_TYPE type;

	union {
		struct ADSB_Frame adsb;

	};
};

#endif
