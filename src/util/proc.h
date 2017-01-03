/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_PROC_H
#define _HAVE_PROC_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool PROC_fork();
void PROC_execRaw(char * const argv[]);
void PROC_execAndFinalize(char * const argv[]);
void PROC_forkAndExec(char * const argv[]);
bool PROC_execAndReturn(char * const argv[]);

#ifdef __cplusplus
}
#endif

#endif
