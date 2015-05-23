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

#ifdef BEAGLEBONE_BLACK
/** GPIO number for CONF_DONE pin of FPGA */
#define FPGA_CONFD (45)
/** GPIO number for DCLK pin of FPGA */
#define FPGA_DCLK (47)
/** GPIO number for nSTATUS pin of FPGA */
#define FPGA_NSTAT (46)
/** GPIO number for DATA0 pin of FPGA */
#define FPGA_DATA0 (44)
/** GPIO number for nCONFIG pin of FPGA */
#define FPGA_NCONF (61)
#else
/** GPIO number for CONF_DONE pin of FPGA */
#define FPGA_CONFD (38)
/** GPIO number for DCLK pin of FPGA */
#define FPGA_DCLK (39)
/** GPIO number for nSTATUS pin of FPGA */
#define FPGA_NSTAT (34)
/** GPIO number for DATA0 pin of FPGA */
#define FPGA_DATA0 (44)
/** GPIO number for nCONFIG pin of FPGA */
#define FPGA_NCONF (37)
#endif
/** Initialize FPGA configuration.
 * \note: GPIO_init() must be called prior to that function!
 */
void FPGA_init()
{
	GPIO_setDirection(FPGA_CONFD, GPIO_DIRECTION_IN);
	GPIO_setDirection(FPGA_DCLK, GPIO_DIRECTION_OUT);
	GPIO_setDirection(FPGA_NSTAT, GPIO_DIRECTION_IN);
	GPIO_setDirection(FPGA_DATA0, GPIO_DIRECTION_OUT);
	GPIO_setDirection(FPGA_NCONF, GPIO_DIRECTION_OUT);

	GPIO_clear(FPGA_DCLK);
}

/** Reset the FPGA configuration.
 * \param timeout waiting time for the reset to happen in units of 50 us
 */
static void reset(uint32_t timeout)
{
	puts("FPGA: Reset");
	GPIO_clear(FPGA_NCONF);
	usleep(50000);
	GPIO_clear(FPGA_DCLK);
	GPIO_set(FPGA_NCONF);

	puts("FPGA: Synchronizing");
	while (timeout--) {
		if (GPIO_read(FPGA_NSTAT))
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
				GPIO_set(FPGA_DATA0);
			else
				GPIO_clear(FPGA_DATA0);
			/* clock that bit via DCLK toggle */
			GPIO_set(FPGA_DCLK);
			GPIO_clear(FPGA_DCLK);
			/* next bit */
			ch >>= 1;
		}
		/* monitor FPGA status */
		if (!GPIO_read(FPGA_NSTAT)) {
			fprintf(stderr, "FPGA: could not program: nSTATUS = 0 "
				"at byte %lu\n", (unsigned long int)pos);
			return false;
		}
	}
	/* done */
	return true;
}

/** (Re-)Program the FPGA.
* \param cfg pointer to buffer configuration, see cfgfile.h
 */
void FPGA_program()
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

		puts("FPGA: Transferring RBF");
		if (!transfer(rfd, st.st_size))
			continue;

		uint32_t spin = CFG_config.fpga.timeout;
		while (GPIO_read(FPGA_CONFD) && spin--)
			usleep(50);
		if (!GPIO_read(FPGA_CONFD)) {
			fprintf(stderr, "FPGA: could not program: CONF_DONE = 0 after "
				"last byte");
			continue;
		}

		munmap(rfd, st.st_size);
		close(fd);

		puts("FPGA: done");
		break;
	}
}
