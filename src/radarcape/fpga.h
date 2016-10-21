/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_FPGA_H
#define _HAVE_FPGA_H

#include <stdbool.h>
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

extern struct Component FPGA_comp;
extern bool FPGA_bbwhite;

#ifdef __cplusplus
}
#endif

#endif
