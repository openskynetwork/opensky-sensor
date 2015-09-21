/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_INPUT_H
#define _HAVE_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

void INPUT_init();
void INPUT_destruct();
void INPUT_connect();
size_t INPUT_read(uint8_t * buf, size_t bufLen);
size_t INPUT_write(uint8_t * buf, size_t bufLen);

#endif
