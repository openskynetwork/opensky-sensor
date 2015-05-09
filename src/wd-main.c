
#include <gpio.h>
#include <watchdog.h>
#include <stdlib.h>

int main(int argc, char * argv[])
{
	/* initialize GPIO subsystem */
	GPIO_init();

	/* Watchdog: initialize and start */
	WATCHDOG_init();
	WATCHDOG_main();

	return EXIT_SUCCESS;
}
