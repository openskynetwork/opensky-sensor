/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_GPIO_H
#define _HAVE_GPIO_H

#include <stdint.h>
#include "util/component.h"

#ifdef __cplusplus
extern "C" {
#endif

enum GPIO_DIRECTION {
	GPIO_DIRECTION_IN,
	GPIO_DIRECTION_OUT
};

extern struct Component GPIO_comp;

void GPIO_setDirection(uint_fast32_t gpio, enum GPIO_DIRECTION dir);
void GPIO_set(uint_fast32_t gpio);
void GPIO_clear(uint_fast32_t gpio);
uint_fast32_t GPIO_read(uint_fast32_t gpio);

#ifdef __cplusplus
}
#endif

#endif