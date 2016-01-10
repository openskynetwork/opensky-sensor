/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_PROC_H
#define _HAVE_PROC_H

#include <stdbool.h>

bool PROC_fork();
void PROC_execRaw(char * argv[]);
void PROC_execAndFinalize(char * argv[]);
void PROC_forkAndExec(char * argv[]);
bool PROC_execAndReturn(char * argv[]);

#endif
