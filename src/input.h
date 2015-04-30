#ifndef _HAVE_INPUT_H
#define _HAVE_INPUT_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <cfgfile.h>

void INPUT_init(const struct CFG_ADSB * cfg);
bool INPUT_connect();
size_t INPUT_read(uint8_t * buf, size_t bufLen);
size_t INPUT_write(uint8_t * buf, size_t bufLen);

#endif
