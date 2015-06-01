#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <fpga.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <cfgfile.h>

#if defined(DEVELOPMENT) && !defined(FWDIR)
#define FWDIR "."
#endif

struct FPGApins {
	const char * name;
	const uint32_t confd;
	const uint32_t dclk;
	const uint32_t nstat;
	const uint32_t data0;
	const uint32_t nconf;
};

enum BB_TYPE {
	BB_TYPE_WHITE = 0,
	BB_TYPE_BLACK = 1
};

static const struct FPGApins pins[2] = {
	[BB_TYPE_WHITE] = {
		.name = "white",
		.confd = 38,
		.dclk = 39,
		.nstat = 34,
		.data0 = 44,
		.nconf = 37
	},
	[BB_TYPE_BLACK] = {
		.name = "black",
		.confd = 45,
		.dclk = 47,
		.nstat = 46,
		.data0 = 44,
		.nconf = 61
	}
};

static const struct FPGApins * fpga;

static void construct();
static bool program();

struct Component FPGA_comp = {
	.description = "FPGA",
	.construct = &construct,
	.start = &program
};

static void reset(uint32_t timeout);
static bool transfer(const uint8_t * rfd, off_t size);

/** Initialize FPGA configuration.
 * \note: GPIO_init() must be called prior to that function!
 */
static void construct(const bool * bbwhite)
{
	fpga = &pins[*bbwhite ? BB_TYPE_WHITE : BB_TYPE_BLACK];

	GPIO_setDirection(fpga->confd, GPIO_DIRECTION_IN);
	GPIO_setDirection(fpga->dclk, GPIO_DIRECTION_OUT);
	GPIO_setDirection(fpga->nstat, GPIO_DIRECTION_IN);
	GPIO_setDirection(fpga->data0, GPIO_DIRECTION_OUT);
	GPIO_setDirection(fpga->nconf, GPIO_DIRECTION_OUT);

	GPIO_clear(fpga->dclk);
}

/** (Re-)Program the FPGA.
* \param cfg pointer to buffer configuration, see cfgfile.h
 */
static bool program()
{
	char file[PATH_MAX];

	struct stat st;
	if (CFG_config.fpga.file[0] == '/' &&
		stat(CFG_config.fpga.file, &st) == 0) {
		strncpy(file, CFG_config.fpga.file, sizeof file);
	} else {
		strncpy(file, FWDIR, PATH_MAX);
		strncat(file, "/", strlen(file) - PATH_MAX);
		strncat(file, CFG_config.fpga.file, strlen(file) - PATH_MAX);
	}

	/* open input file */
	int fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		error(-1, errno, "FPGA: could not open '%s'", file);

	/* stat input file for its size */
	if (fstat(fd, &st) < 0) {
		close(fd);
		error(-1, errno, "FPGA: could not stat '%s'", file);
	}

	/* mmap input file */
	uint8_t * rfd = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (rfd == MAP_FAILED) {
		close(fd);
		error(-1, errno, "FPGA: could not mmap '%s'", file);
	}

	uint32_t try;
	uint32_t retries = CFG_config.fpga.retries + 1;
	for (try = 1; try < retries; ++try) {
		printf("FPGA: Programming (attempt %" PRIu32 " of %" PRIu32 ")\n", try,
			retries);

		/* reset FPGA */
		reset(CFG_config.fpga.timeout);

		printf("FPGA: Transferring RBF for beagle bone %s\n", fpga->name);
		if (!transfer(rfd, st.st_size))
			continue;

		uint32_t spin = CFG_config.fpga.timeout;
		while (GPIO_read(fpga->confd) && spin--)
			usleep(50);
		if (!GPIO_read(fpga->confd)) {
			fprintf(stderr, "FPGA: could not program: CONF_DONE = 0 after "
				"last byte");
			continue;
		}

		munmap(rfd, st.st_size);
		close(fd);

		puts("FPGA: done");
		break;
	}
	return true;
}

/** Reset the FPGA configuration.
 * \param timeout waiting time for the reset to happen in units of 50 us
 */
static void reset(uint32_t timeout)
{
	puts("FPGA: Reset");
	GPIO_clear(fpga->nconf);
	usleep(50000);
	GPIO_clear(fpga->dclk);
	GPIO_set(fpga->nconf);

	puts("FPGA: Synchronizing");
	while (timeout--) {
		if (GPIO_read(fpga->nstat))
			return;
		usleep(50);
	}
	error(-1, 0, "FPGA: could not synchronize with the FPGA");
}

/** Transfer data to the FPGA's configuration interface
 * \param rfd data to transfer
 * \param size size of data
 * \param true if transfer done without any error
 */
static bool transfer(const uint8_t * rfd, off_t size)
{
	/* transfer until done */
	off_t pos;
	for (pos = 0; pos < size; ++pos) {
		uint8_t ch = rfd[pos];
		uint32_t bit;
		for (bit = 0; bit < 8; ++bit) {
			/* transfer next bit via DATA0 line */
			if (ch & 1)
				GPIO_set(fpga->data0);
			else
				GPIO_clear(fpga->data0);
			/* clock that bit via DCLK toggle */
			GPIO_set(fpga->dclk);
			GPIO_clear(fpga->dclk);
			/* next bit */
			ch >>= 1;
		}
		/* monitor FPGA status */
		if (!GPIO_read(fpga->nstat)) {
			fprintf(stderr, "FPGA: could not program: nSTATUS = 0 "
				"at byte %lu\n", (unsigned long int)pos);
			return false;
		}
	}
	/* done */
	return true;
}
