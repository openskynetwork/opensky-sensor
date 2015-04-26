#ifndef _HAVE_NETWORK_H
#define _HAVE_NETWORK_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <adsb.h>

void NET_init(const char * server, uint16_t port, uint32_t reconnect,
	uint32_t serial);
void NET_main();

void NET_sync_send();
bool NET_sendFrame(const struct ADSB_Frame * frame);
bool NET_sendTimeout();

void NET_sync_recv();
ssize_t NET_receive(uint8_t * buf, size_t len);

#endif
