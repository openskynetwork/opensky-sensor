
#include "gpio.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define FPGA_CONFD 13
#define FPGA_DCLK 15
#define FPGA_NSTAT 14
#define FPGA_DATA0 12
#define FPGA_NCONF 5

void FPGA_init()
{
	GPIO_setDirection(FPGA_CONFD, GPIO_DIRECTION_IN);
	GPIO_setDirection(FPGA_DCLK, GPIO_DIRECTION_OUT);
	GPIO_setDirection(FPGA_NSTAT, GPIO_DIRECTION_IN);
	GPIO_setDirection(FPGA_DATA0, GPIO_DIRECTION_OUT);
	GPIO_setDirection(FPGA_NCONF, GPIO_DIRECTION_OUT);

	GPIO_clear(FPGA_DCLK);
}

void FPGA_reset()
{
	GPIO_clear(FPGA_NCONF);
	GPIO_clear(FPGA_DCLK);
	GPIO_set(FPGA_NCONF);

	while (GPIO_read(FPGA_NSTAT));
}

void FPGA_program(const char * file)
{
	int fd = open(file, O_RDWR);
	if (fd < 0)
		error(-1, errno, "Could not open '%s'", file);

	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		error(-1, errno, "Could not stat '%s'", file);
	}

	char * rfd = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

	FPGA_reset();
	off_t pos;
	for (pos = 0; pos < st.st_size; ++pos) {
		char ch = rfd[pos];
		uint32_t bit;
		for (bit = 0; bit < 8; ++bit) {
			if (ch & 1)
				GPIO_set(FPGA_DATA0);
			else
				GPIO_clear(FPGA_DATA0);
			GPIO_set(FPGA_DCLK);
			GPIO_clear(FPGA_DCLK);
			ch >>= 1;
		}
		if (!GPIO_read(FPGA_NSTAT)) {
			munmap(rfd, st.st_size);
			close(fd);
			error(-1, 0, "Could not program FPGA: nSTATUS = 0 at byte %lu",
				(long unsigned)pos);
		}
	}

	munmap(rfd, st.st_size);
	close(fd);

	if (!GPIO_read(FPGA_CONFD))
		error(-1, 0, "Could not program FPGA: CONF_DONE = 0 after last byte");
}
