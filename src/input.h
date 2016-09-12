/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_INPUT_H
#define _HAVE_INPUT_H

#include <stdbool.h>
#include <stdint.h>
#include "component.h"
#include "openskytypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const struct Component INPUT_comp;

void INPUT_init();
void INPUT_destruct();
void INPUT_connect();
void INPUT_reconfigure();
bool INPUT_getFrame(struct OPENSKY_RawFrame * raw,
	struct OPENSKY_DecodedFrame * decoded);

#ifdef __cplusplus
}
#endif

#endif
