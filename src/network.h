#ifndef _HAVE_NETWORK_H
#define _HAVE_NETWORK_H

#include <stdbool.h>
#include <adsb.h>

bool NET_connect(const char * server, int port);
bool NET_sendSerial();
bool NET_sendFrame(const struct ADSB_Frame * frame);
bool NET_sendTimeout();

#endif
