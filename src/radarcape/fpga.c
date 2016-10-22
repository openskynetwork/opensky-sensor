/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include "gpio.h"
#include "fpga.h"
#include "util/log.h"
#include "util/cfgfile.h"

static const char PFX[] = "FPGA";

#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(FWDIR)
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
		.nconf = 37 },
	[BB_TYPE_BLACK] = {
		.name = "black",
		.confd = 45,
		.dclk = 47,
		.nstat = 46,
		.data0 = 44,
		.nconf = 61 } };

static const struct FPGApins * fpga;

static char cfgFilename[PATH_MAX];
static bool cfgConfigure;

bool FPGA_bbwhite;

#define RETRIES 3
#define TIMEOUT 10

static bool checkCfg(const struct CFG_Section * sect);

static struct CFG_Section cfg = {
	.name = "FPGA",
	.check = &checkCfg,
	.n_opt = 2,
	.options = {
		{
			.name = "File",
			.type = CFG_VALUE_TYPE_STRING,
			.var = cfgFilename,
			.maxlen = sizeof(cfgFilename),
		},
		{
			.name = "Configure",
			.type = CFG_VALUE_TYPE_BOOL,
			.var = &cfgConfigure,
		}
	}
};

static bool construct();
static bool program();

struct Component FPGA_comp = {
	.description = PFX,
	.onConstruct = &construct,
	.onStart = &program,
	.config = &cfg,
	.enabled = &cfgConfigure,
	.dependencies = {
		&GPIO_comp,
		NULL
	}
};

static bool reset(uint32_t timeout);
static bool transfer(const uint8_t * rfd, off_t size);

static bool checkCfg(const struct CFG_Section * sect)
{
	assert(sect == &cfg);
	if (cfgConfigure && cfgFilename[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "FPGA.file is missing");
		return false;
	}
	return true;
}

/** Initialize FPGA configuration.
 * \note: GPIO_init() must be called prior to that function!
 */
static bool construct()
{
	CFG_registerSection(&cfg);

	fpga = &pins[FPGA_bbwhite ? BB_TYPE_WHITE : BB_TYPE_BLACK];

	GPIO_setDirection(fpga->confd, GPIO_DIRECTION_IN);
	GPIO_setDirection(fpga->dclk, GPIO_DIRECTION_OUT);
	GPIO_setDirection(fpga->nstat, GPIO_DIRECTION_IN);
	GPIO_setDirection(fpga->data0, GPIO_DIRECTION_OUT);
	GPIO_setDirection(fpga->nconf, GPIO_DIRECTION_OUT);

	GPIO_clear(fpga->dclk);

	return true;
}

/** (Re-)Program the FPGA.
 * \param cfg pointer to buffer configuration, see cfgfile.h
 */
static bool program()
{
	char file[PATH_MAX];

	struct stat st;
	if (cfgFilename[0] == '/' && stat(cfgFilename, &st) == 0) {
		strncpy(file, cfgFilename, sizeof file);
	} else {
		strncpy(file, FWDIR, sizeof file);
		strncat(file, "/", strlen(file) - sizeof file);
		strncat(file, cfgFilename, strlen(file) - sizeof file);
	}

	/* open input file */
	int fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "could not open '%s'", file);
		return false;
	}

	/* stat input file for its size */
	if (fstat(fd, &st) < 0) {
		close(fd);
		LOG_errno(LOG_LEVEL_ERROR, PFX, "could not stat '%s'", file);
		return false;
	}

	/* mmap input file */
	uint8_t * rfd = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (rfd == MAP_FAILED) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "could not mmap '%s'", file);
		return false;
	}

	uint32_t try;
	uint32_t retries = RETRIES + 1;
	for (try = 1; try < retries; ++try) {
		LOG_logf(LOG_LEVEL_INFO, PFX, "Programming (attempt %" PRIu32
			" of %" PRIu32 ")", try, RETRIES);

		/* reset FPGA */
		if (!reset(TIMEOUT))
			continue;

		LOG_logf(LOG_LEVEL_INFO, PFX, "Transferring RBF for beagle bone %s",
			fpga->name);
		if (!transfer(rfd, st.st_size))
			continue;

		uint32_t spin = TIMEOUT;
		while (GPIO_read(fpga->confd) && spin--)
			usleep(50);
		if (!GPIO_read(fpga->confd)) {
			LOG_log(LOG_LEVEL_WARN, PFX, "could not program: CONF_DONE = 0"
				"after last byte");
			continue;
		}

		munmap(rfd, st.st_size);

		LOG_log(LOG_LEVEL_INFO, PFX, "done");
		break;
	}
	if (try == retries) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "could not program fpga after %" PRIu32
			" attempts", retries);
		return false;
	}
	return true;
}

/** Reset the FPGA configuration.
 * \param timeout waiting time for the reset to happen in units of 50 us
 */
static bool reset(uint32_t timeout)
{
	LOG_log(LOG_LEVEL_DEBUG, PFX, "Reset");
	GPIO_clear(fpga->nconf);
	usleep(50000);
	GPIO_clear(fpga->dclk);
	GPIO_set(fpga->nconf);

	LOG_log(LOG_LEVEL_DEBUG, PFX, "Synchronizing");
	while (timeout--) {
		if (GPIO_read(fpga->nstat))
			return true;
		usleep(50);
	}
	LOG_logf(LOG_LEVEL_ERROR, PFX, "could not synchronize with the FPGA");
	return false;
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
			LOG_logf(LOG_LEVEL_WARN, PFX, "could not program: nSTATUS = 0 "
				"at byte %lu", (unsigned long int)pos);
			return false;
		}
	}
	/* done */
	return true;
}