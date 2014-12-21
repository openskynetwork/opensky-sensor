
#include <gpio.h>
#include <watchdog.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>

int main()
{
	GPIO_init();

	WATCHDOG_init();

	pthread_t watchdog;
	int success;
	if (pthread_create(&watchdog, NULL, &WATCHDOG_main, NULL) != 0) {
		error(-1, errno, "Could not create watchdog thread");
	}

	return 0;
}

