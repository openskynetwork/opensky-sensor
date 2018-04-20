/* Copyright (c) 2015-2018 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_DEVTYPE_H
#define _HAVE_DEVTYPE_H

#include "core/openskytypes.h"
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

enum OPENSKY_DEVICE_TYPE DEVTYPE_getDeviceType();

extern const struct Component DEVTYPE_comp;

#ifdef __cplusplus
}
#endif

#endif
