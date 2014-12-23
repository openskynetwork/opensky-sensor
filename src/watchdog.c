
#include <gpio.h>
#include <stdio.h>
#include <unistd.h>

/** Watchdog GPIO number */
#define WD_GPIO 60
/** Watchdog repetition rate in seconds */
#define WD_REPEAT 30

/** Initialize watchdog.
 * \note: GPIO_init() must be called prior to that function!
 */
void WATCHDOG_init()
{
	GPIO_setDirection(WD_GPIO, GPIO_DIRECTION_OUT);
	GPIO_clear(WD_GPIO);
}

/** Watchdog mainloop: tell the watchdog we are still here in an endless loop */
void WATCHDOG_main()
{
	while (1) {
		puts("WD: keepalive");
		GPIO_set(WD_GPIO);
		GPIO_clear(WD_GPIO);
		sleep(WD_REPEAT);
	}
}
