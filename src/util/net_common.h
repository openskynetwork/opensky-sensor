/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_NET_COMMON_H
#define _HAVE_NET_COMMON_H

#include <stdint.h>
#include "port/socket.h"

#ifdef __cplusplus
extern "C" {
#endif

sock_t NETC_connect(const char * prefix, const char * hostName, uint16_t port);

#ifdef __cplusplus
}
#endif

#endif
