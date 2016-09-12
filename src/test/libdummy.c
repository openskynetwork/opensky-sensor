/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <stdlib.h>
#include "../component.h"
#include "../buffer.h"

struct Component TB_comp = { .description = "TB_D", .dependencies = { NULL } };
struct Component RELAY_comp = { .description = "RELAY_D",
	.dependencies = { &BUF_comp, NULL } };
