
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

/** GPIO number for CONF_DONE pin of FPGA */
#define FPGA_CONFD (32 + 6)
/** GPIO number for DCLK pin of FPGA */
#define FPGA_DCLK (32 + 7)
/** GPIO number for nSTATUS pin of FPGA */
#define FPGA_NSTAT (32 + 2)
/** GPIO number for DATA0 pin of FPGA */
#define FPGA_DATA0 (32 + 12)
/** GPIO number for nCONFIG pin of FPGA */
#define FPGA_NCONF (32 + 5)

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
 * \param timeout waiting time for the reset to happen in units of 50 ms
 */
void FPGA_reset(uint32_t timeout)
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
		usleep(500000);
	}
	error(-1, 0, "FPGA: could not synchronize with the FPGA");
}

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
	return true;
}

/** (Re-)Program the FPGA.
 * \param file path to file which is transferred to the FPGA
 * \param timeout timeout for I/O waiting loops in units of 50 ms
 * \param retries number of retries. 0 means "one try"
 */
void FPGA_program(const char * file, uint32_t timeout, uint32_t retries)
{
	/* open input file */
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		error(-1, errno, "FPGA: could not open '%s'", file);

	/* stat input file for its size */
	struct stat st;
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
	++retries;
	for (try = 1; try < retries; ++try) {
		printf("FPGA: Programming (attempt %" PRIu32 " of %" PRIu32 ")\n", try,
			retries);

		/* reset FPGA */
		FPGA_reset(timeout);

		puts("FPGA: Transferring RBF");
		if (!transfer(rfd, st.st_size))
			continue;

		uint32_t spin = timeout;
		while (GPIO_read(FPGA_CONFD) && spin--)
			usleep(50000);
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
