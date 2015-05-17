#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>

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

static void controllerInit(int devmem, struct Controller * ctrl);
static inline struct Controller * getController(uint32_t gpio);
static inline uint32_t getBit(uint32_t gpio);

/** Initialize the GPIO subsystem.
 * \note Must be called exactly once before any GPIO functions are used! */
void GPIO_init()
{
	int devmem;

	/* open /dev/mem */
	devmem = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC);
	if (devmem < 0)
		error(-1, errno, "GPIO: Could not open /dev/mem");

	/* initialize all 4 controllers */
	int i;
	for (i = 0; i < 4; ++i)
		controllerInit(devmem, &controllers[i]);
}

/** Set Direction of a GPIO.
 * \param gpio gpio number
 * \param dir direction (either in or out)
 */
void GPIO_setDirection(uint32_t gpio, enum GPIO_DIRECTION dir)
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
void GPIO_set(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->set = getBit(gpio);
	(void)*ctrl->in; // settle
}

/** Clear an output GPIO.
 * \param gpio gpio number
 */
void GPIO_clear(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	*ctrl->clear = getBit(gpio);
	(void)*ctrl->in; // settle
}

/** Read an input GPIO.
 * \param gpio gpio number
 * \return 1 if gpio is set, 0 otherwise
 */
uint32_t GPIO_read(uint32_t gpio)
{
	struct Controller * ctrl = getController(gpio);
	return !!(*ctrl->in & getBit(gpio));
}

/** Initialize a GPIO controller.
 * \param devmem file handle to /dev/mem
 * \param ctrl controller to be initialized
 */
static void controllerInit(int devmem, struct Controller * ctrl)
{
	ctrl->map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devmem,
		ctrl->base);

	if (ctrl->map == (char*)-1) {
		close(devmem);
		error(-1, errno, "GPIO: Could not mmap /dev/mem for base 0x%08" PRIxPTR,
			ctrl->base);
	}

	ctrl->set = (volatile uint32_t*)(ctrl->map + 0x194);
	ctrl->clear = (volatile uint32_t*)(ctrl->map + 0x190);
	ctrl->oe = (volatile uint32_t*)(ctrl->map + 0x134);
	ctrl->in = (volatile uint32_t*)(ctrl->map + 0x138);
}

/** Get GPIO controller from gpio number
 * \param gpio gpio number
 * \return GPIO controller
 */
static inline struct Controller * getController(uint32_t gpio)
{
	return &controllers[gpio >> 5];
}

/** Get Data Bit from gpio number
 * \param gpio gpio number
 * \return gpio data bit
 */
static inline uint32_t getBit(uint32_t gpio)
{
	return 1 << (gpio & 31);
}
