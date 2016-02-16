/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifndef _HAVE_NET_COMMON_H
#define _HAVE_NET_COMMON_H

#include <stdint.h>

int NETC_connect(const char * component, const char * hostName, uint16_t port);

#endif
