/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_PROC_H
#define _HAVE_PROC_H

#include <stdbool.h>

bool PROC_fork();
void PROC_execRaw(char * argv[]);
void PROC_execAndFinalize(char * argv[]);
void PROC_forkAndExec(char * argv[]);
bool PROC_execAndReturn(char * argv[]);

#endif
