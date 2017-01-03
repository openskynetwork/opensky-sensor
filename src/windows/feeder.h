/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef HAVE_FEEDER_H
#define HAVE_FEEDER_H

#ifdef __cplusplus
extern "C" {
#endif

void FEEDER_init();
void FEEDER_configure();
void FEEDER_start();
void FEEDER_stop();
void FEEDER_cleanup();

#ifdef __cplusplus
}
#endif

#endif
