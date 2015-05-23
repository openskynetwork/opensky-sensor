#ifndef _HAVE_GPIO_H
#define _HAVE_GPIO_H

#include <stdint.h>
#include <component.h>

enum GPIO_DIRECTION {
	GPIO_DIRECTION_IN,
	GPIO_DIRECTION_OUT
};

extern struct Component GPIO_comp;

void GPIO_setDirection(uint32_t gpio, enum GPIO_DIRECTION dir);
void GPIO_set(uint32_t gpio);
void GPIO_clear(uint32_t gpio);
uint32_t GPIO_read(uint32_t gpio);

#endif
