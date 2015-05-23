#ifndef _HAVE_INPUT_H
#define _HAVE_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

void INPUT_init();
bool INPUT_connect();
size_t INPUT_read(uint8_t * buf, size_t bufLen);
size_t INPUT_write(uint8_t * buf, size_t bufLen);

#endif
