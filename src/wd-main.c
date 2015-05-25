#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <watchdog.h>
#include <stdlib.h>

int main(int argc, char * argv[])
{
	/* initialize GPIO subsystem */
	GPIO_comp.construct(NULL);

	WD_comp.construct(NULL);
	WD_comp.main();

	return EXIT_SUCCESS;
}
