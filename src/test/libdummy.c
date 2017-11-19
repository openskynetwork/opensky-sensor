/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include "core/buffer.h"
#include "util/component.h"

struct Component TB_comp = { .description = "TB_D", .dependencies = { NULL } };
struct Component RELAY_comp = { .description = "RELAY_D",
	.dependencies = { &BUF_comp, NULL } };

void GPS_reset() {}

void GPS_resetNeedFlag() {}

void GPS_setPosition(double latitude, double longitude, double altitude) {}

void GPS_setPositionWithFix(double latitude, double longitude, double altitude) {}

void GPS_setHasFix(bool fix) {}

