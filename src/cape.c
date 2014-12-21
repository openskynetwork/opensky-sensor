
#include <gpio.h>
#include <watchdog.h>
#include <fpga.h>
#include <pthread.h>
#include <stdlib.h>
#include <error.h>
#include <errno.h>

typedef void*(*PTHREAD_FN)(void*);

int main()
{
	GPIO_init();

	WATCHDOG_init();

	pthread_t watchdog;
	int success;
	if (pthread_create(&watchdog, NULL, (PTHREAD_FN)&WATCHDOG_main, NULL)) {
		error(-1, errno, "Could not create watchdog thread");
	}

	FPGA_init();
	FPGA_program("cape.rtf");

	return 0;
}

