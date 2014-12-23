
#include "gpio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

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

/** Reset the FPGA configuration. */
void FPGA_reset()
{
	puts("FPGA: Reset");
	GPIO_clear(FPGA_NCONF);
	usleep(50000);
	GPIO_clear(FPGA_DCLK);
	GPIO_set(FPGA_NCONF);

	puts("FPGA: Synchronizing");
	while (!GPIO_read(FPGA_NSTAT))
		usleep(500000);
}

/** (Re-)Program the FPGA.
 * \param file path to file which is transferred to the FPGA
 */
void FPGA_program(const char * file)
{
	/* open input file */
	int fd = open(file, O_RDWR);
	if (fd < 0)
		error(-1, errno, "Could not open '%s'", file);

	/* stat input file for its size */
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		error(-1, errno, "Could not stat '%s'", file);
	}

	/* mmap input file */
	char * rfd = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);


	/* reset FPGA */
	FPGA_reset();

	puts("FPGA: Programming");

	/* transfer until done */
	off_t pos;
	for (pos = 0; pos < st.st_size; ++pos) {
		char ch = rfd[pos];
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
			munmap(rfd, st.st_size);
			close(fd);
			error(-1, 0, "Could not program FPGA: nSTATUS = 0 at byte %lu",
				(long unsigned)pos);
		}
	}

	munmap(rfd, st.st_size);
	close(fd);

	puts("FPGA: Done");

	/* check FPGA status */
	if (!GPIO_read(FPGA_CONFD))
		error(-1, 0, "Could not program FPGA: CONF_DONE = 0 after last byte");
}
