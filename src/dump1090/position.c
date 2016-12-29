/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "core/gps.h"
#include "util/cfgfile.h"
#include "util/component.h"
#include "util/log.h"

/** Component: Prefix */
static const char PFX[] = "POS";

/** Configuration: latitude given */
static bool hasLatitude;
/** Configuration: longitude given */
static bool hasLongitude;
/** Configuration: altitude given */
static bool hasAltitude;

/** Configuration: latitude */
static double latitude;
/** Configuration: longitude */
static double longitude;
/** Configuration: altitude */
static double altitude;

static bool checkCfg(const struct CFG_Section * sect);
static void setPosition();

/** Configuration Descriptor */
static const struct CFG_Section cfgDesc = {
	.name = "GPS",
	.check = &checkCfg,
	.n_opt = 3,
	.options = {
		{
			.name = "Latitude",
			.type = CFG_VALUE_TYPE_DOUBLE,
			.var = &latitude,
			.given = &hasLatitude,
		},
		{
			.name = "Longitude",
			.type = CFG_VALUE_TYPE_DOUBLE,
			.var = &longitude,
			.given = &hasLongitude,
		},
		{
			.name = "Altitude",
			.type = CFG_VALUE_TYPE_DOUBLE,
			.var = &altitude,
			.given = &hasAltitude,
		},
	}
};

/** Component Descriptor */
const struct Component GPS_comp = {
	.description = PFX,
	.config = &cfgDesc,
	.main = &setPosition,
	.dependencies = { NULL }
};

/** Component entry point: set position */
static void setPosition()
{
	GPS_setPosition(latitude, longitude, altitude);
}

/** Check configuration
 * @param sect should be @cfgDesc
 * @return true if configuration is sane
 */
static bool checkCfg(const struct CFG_Section * sect)
{
	size_t i;
	for (i = 0; i < cfgDesc.n_opt; ++i) {
		const struct CFG_Option * opt = &cfgDesc.options[i];
		if (!*opt->given)
			LOG_logf(LOG_LEVEL_EMERG, PFX, "%s is not configured", opt->name);
	}

	if (latitude < -180 || latitude > 180)
		LOG_log(LOG_LEVEL_EMERG, PFX, "Latitude must be given within +/-180 "
			"degrees");

	if (longitude < -90 || longitude > 90)
		LOG_log(LOG_LEVEL_EMERG, PFX, "Longitude must be given within +/-90 "
			"degrees");

	return true;
}
