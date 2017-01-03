/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_NETWORK_H
#define _HAVE_NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

struct NET_Statistics {
	bool isOnline;
	uint64_t onlineSecs;
	uint64_t disconnects;
	uint64_t connectionAttempts;
	uint64_t bytesSent;
	uint64_t bytesReceived;
};

extern const struct Component NET_comp;

void NET_waitConnected();

void NET_forceDisconnect();

bool NET_checkConnected();

bool NET_send(const void * buf, size_t len);

ssize_t NET_receive(uint8_t * buf, size_t len);

void NET_getStatistics(struct NET_Statistics * statistics);

#ifdef __cplusplus
}
#endif

#endif
