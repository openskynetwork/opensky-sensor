/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_RC_PARSER_H
#define _HAVE_RC_PARSER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct RC_PARSER_Statistics {
	uint64_t outOfSync;
	uint64_t frameTypeUnknown;
};

void RC_PARSER_getStatistics(struct RC_PARSER_Statistics * statistics);

#ifdef __cplusplus
}
#endif

#endif
