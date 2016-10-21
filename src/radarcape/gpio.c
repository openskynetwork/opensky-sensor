/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gpio.h"
#include "util/util.h"
#include "util/log.h"

static const char PFX[] = "GPIO";

/** Represents a GPIO controller */
struct Controller {
	/** controller base address */
	const uintptr_t base;

	/** mapped base address */
	char * map;
	/** shortcut for DATA SET register */
	volatile uint32_t * set;
	/** shortcut for DATA CLEAR register */
	volatile uint32_t * clear;
	/** shortcut for OUTPUT ENABLE register */
	volatile uint32_t * oe;
	/** shortcut for DATAIN register */
	volatile uint32_t * in;
};

/** All 4 GPIO controllers with their base addresses */
static struct Controller controllers[4] = {
	[0] = { .base = 0x44e07000 },
	[1] = { .base = 0x4804c000 },
	[2] = { .base = 0x481ac000 },
	[3] = { .base = 0x481ae000 }
};

static bool construct();
static void destruct();

struct Component GPIO_comp = {
	.description = PFX,
	.onConstruct = &construct,
	.onDestruct = &destruct,
	.dependencies = { NULL }
};

static bool controllerInit(int devfd, struct Controller * ctrl);
static void controllerDestruct(struct Controller * ctrl);
static inline struct Controller * getController(uint_fast32_t gpio);
static inline uint_fast32_t getBit(uint_fast32_t gpio);

/** Initialize the GPIO subsystem.
 * \note Must be called exactly once before any GPIO functions are used! */
static bool construct()
{
	/* open /dev/mem */
	int devfd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (devfd < 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "Could not open /dev/mem");
		return false;
	}

	/* initialize all 4 controllers */
	size_t i;
	for (i = 0; i < ARRAY_SIZE(controllers); ++i) {
		if (!controllerInit(devfd, &controllers[i])) {
			size_t j;
			for (j = 0; j < i; ++j)
				controllerDestruct(&controllers[j]);
			return false;
		}
	}

	close(devfd);

	return true;
}

static void destruct()
{
	/* terminate all 4 controllers */
	size_t i;
	for (i = 0; i < ARRAY_SIZE(controllers); ++i)
		controllerDestruct(&controllers[i]);
}

/** Set Direction of a GPIO.
 * \param gpio gpio number
 * \param dir direction (either in or out)
 */
void GPIO_setDirection(uint_fast32_t gpio, enum GPIO_DIRECTION dir)
{
	struct Controller * ctrl = getController(gpio);
	if (dir == GPIO_DIRECTION_OUT)
		*ctrl->oe &= ~getBit(gpio); // cleared bits are outputs
	else
		*ctrl->oe |= getBit(gpio); // set bits are inputs
	(void)*ctrl->oe; // settle
}

/** Set an output GPIO.
 * \param gpio gpio number
 */
void GPIO_set(uint_fast32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->set = getBit(gpio);
	(void)*ctrl->in; // settle
}

/** Clear an output GPIO.
 * \param gpio gpio number
 */
void GPIO_clear(uint_fast32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->clear = getBit(gpio);
	(void)*ctrl->in; // settle
}

/** Read an input GPIO.
 * \param gpio gpio number
 * \return 1 if gpio is set, 0 otherwise
 */
uint_fast32_t GPIO_read(uint_fast32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	return !!(*ctrl->in & getBit(gpio));
}

/** Initialize a GPIO controller.
 * \param devfd file handle to /dev/mem
 * \param ctrl controller to be initialized
 */
static bool controllerInit(int devfd, struct Controller * ctrl)
{
	ctrl->map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devfd,
		ctrl->base);

	if (ctrl->map == MAP_FAILED) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not mmap /dev/mem for base 0x%08"
			PRIxPTR, ctrl->base);
		return false;
	}

	ctrl->set = (volatile uint32_t*)(ctrl->map + 0x194);
	ctrl->clear = (volatile uint32_t*)(ctrl->map + 0x190);
	ctrl->oe = (volatile uint32_t*)(ctrl->map + 0x134);
	ctrl->in = (volatile uint32_t*)(ctrl->map + 0x138);

	return true;
}

static void controllerDestruct(struct Controller * ctrl)
{
	munmap(ctrl->map, 4096);
}

/** Get GPIO controller from gpio number
 * \param gpio gpio number
 * \return GPIO controller
 */
static inline struct Controller * getController(uint_fast32_t gpio)
{
	return &controllers[gpio >> 5];
}

/** Get Data Bit from gpio number
 * \param gpio gpio number
 * \return gpio data bit
 */
static inline uint_fast32_t getBit(uint_fast32_t gpio)
{
	return 1 << (gpio & 31);
}
