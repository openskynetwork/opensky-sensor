#ifndef _HAVE_GPIO_H
#define _HAVE_GPIO_H

#include <stdint.h>

enum GPIO_DIRECTION {
	GPIO_DIRECTION_IN,
	GPIO_DIRECTION_OUT
};

void GPIO_init();
void GPIO_setDirection(uint32_t gpio, enum GPIO_DIRECTION dir);
void GPIO_set(uint32_t gpio);
void GPIO_clear(uint32_t gpio);
uint32_t GPIO_read(uint32_t gpio);

#endif
