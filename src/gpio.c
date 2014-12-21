
#include <gpio.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

struct Controller {
	const uintptr_t base;

	char * map;
	volatile uint32_t * set;
	volatile uint32_t * clear;
	volatile uint32_t * oe;
	volatile uint32_t * in;
};

static struct Controller controllers[4] = {
	[0] = { .base = 0x44e07000 },
	[1] = { .base = 0x4804c000 },
	[2] = { .base = 0x481ac000 },
	[3] = { .base = 0x481ae000 }
};

static void controllerInit(int devmem, struct Controller * ctrl)
{
	ctrl->map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devmem,
		ctrl->base);

	if (ctrl->map == (char*)-1) {
		close(devmem);
		error(-1, errno, "Could not mmap /dev/mem for base 0x%08" PRIxPTR,
			ctrl->base);
	}

	ctrl->set = (volatile uint32_t*)(ctrl->map + 0x194);
	ctrl->clear = (volatile uint32_t*)(ctrl->map + 0x190);
	ctrl->oe = (volatile uint32_t*)(ctrl->map + 0x134);
	ctrl->in = (volatile uint32_t*)(ctrl->map + 0x138);
}

static inline struct Controller * getController(uint32_t gpio)
{
	return &controllers[gpio >> 5];
}

static inline uint32_t getBit(uint32_t gpio)
{
	return 1 << (gpio & 31);
}

void GPIO_init()
{
	int devmem;

	/* open file */
	devmem = open("/dev/mem", O_RDWR | O_SYNC);
	if (devmem < 0)
		error(-1, errno, "Could not open /dev/mem");

	int i;
	for (i = 0; i < 4; ++i)
		controllerInit(devmem, &controllers[i]);
}

void GPIO_setDirection(uint32_t gpio, enum GPIO_DIRECTION dir)
{
	struct Controller * ctrl = getController(gpio);
	if (dir == GPIO_DIRECTION_OUT)
		*ctrl->oe &= ~getBit(gpio);
	else
		*ctrl->oe |= getBit(gpio);
	(void)*ctrl->oe;
}

void GPIO_set(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->set = getBit(gpio);
	(void)*ctrl->in;
}

void GPIO_clear(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->clear = getBit(gpio);
	(void)*ctrl->in;
}

uint32_t GPIO_read(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	return !!(*ctrl->in & getBit(gpio));
}
