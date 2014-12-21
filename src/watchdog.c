
#include <gpio.h>

#define WD_GPIO 60
#define WD_REPEAT 30

void WATCHDOG_init()
{
	GPIO_setDirection(WD_GPIO, GPIO_DIRECTION_OUT);
	GPIO_clear(WD_GPIO);
}

void WATCHDOG_main()
{
	while (1) {
		GPIO_set(WD_GPIO);
		GPIO_clear(WD_GPIO);
		sleep(WD_REPEAT);
	}
}
