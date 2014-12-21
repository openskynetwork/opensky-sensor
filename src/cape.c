
#include <gpio.h>
#include <watchdog.h>

int main()
{
	GPIO_init();

	WATCHDOG_init();

	WATCHDOG_main();

	return 0;
}

