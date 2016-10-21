/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <stdlib.h>
#include "core/buffer.h"
#include "util/component.h"

struct Component TB_comp = { .description = "TB_D", .dependencies = { NULL } };
struct Component RELAY_comp = { .description = "RELAY_D",
	.dependencies = { &BUF_comp, NULL } };
