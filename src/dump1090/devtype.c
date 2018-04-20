/* Copyright (c) 2015-2018 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include "devtype.h"
#include "util/cfgfile.h"

/** Component: Prefix */
static const char PFX[] = "DEVTYPE";

static char devType[20];

/** Configuration Descriptor */
static struct CFG_Section cfgDesc =
{
	.name = "Device",
	.n_opt = 1,
	.options = {
		{
			.name = "Type",
			.type = CFG_VALUE_TYPE_STRING,
			.var = &devType,
			.maxlen = sizeof(devType) - 1,
		}
	}
};

/** Component Descriptor */
const struct Component DEVTYPE_comp =
{
	.description = PFX,
	.config = &cfgDesc,
	.dependencies = { NULL }
};

/** Get device type for dump1090 feeder (essentially: branch of dump1090) */
enum OPENSKY_DEVICE_TYPE DEVTYPE_getDeviceType()
{
	if (!strncasecmp(devType, "hptoa", sizeof(devType)) ||
			!strncasecmp(devType, "opensky", sizeof(devType))) {
		return OPENSKY_DEVICE_TYPE_FEEDER_HPTOA;
	} else {
		return OPENSKY_DEVICE_TYPE_FEEDER;
	}
}

