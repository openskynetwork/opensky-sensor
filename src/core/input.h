/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_INPUT_H
#define _HAVE_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "openskytypes.h"
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct Component INPUT_comp;

void INPUT_connect();
void INPUT_disconnect();
void INPUT_reconfigure();
bool INPUT_getFrame(struct OPENSKY_RawFrame * raw,
	struct OPENSKY_DecodedFrame * decoded);

#ifdef __cplusplus
}
#endif

#endif
