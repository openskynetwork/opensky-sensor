#ifndef _HAVE_CFGFILE_H
#define _HAVE_CFGFILE_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

struct CFG_WD {
	bool enabled;
};

struct CFG_FPGA {
	bool configure;
	char file[PATH_MAX];
	uint32_t retries;
	uint32_t timeout;
};

struct CFG_ADSB {
	char uart[PATH_MAX];

	bool frameFilter;
	bool crc;
	bool timestampGPS;
	bool rts;
	bool fec;
	bool modeAC;

	bool modeSLong;
	bool modeSShort;
};

struct CFG_NET {
	char host[255];
	uint16_t port;
	uint32_t timeout;
	uint32_t reconnectInterval;
};

struct CFG_BUF {
	bool history;
	uint32_t statBacklog;
	uint32_t dynBacklog;
	uint32_t dynIncrement;
	bool gcEnabled;
	uint32_t gcInterval;
	uint32_t gcLevel;
};

struct CFG_DEV {
	uint32_t serial;
	bool serialSet;
};

struct CFG_Config {
	struct CFG_WD wd;
	struct CFG_FPGA fpga;
	struct CFG_ADSB adsb;
	struct CFG_NET net;
	struct CFG_BUF buf;
	struct CFG_DEV dev;
};

void CFG_read(const char * file, struct CFG_Config * cfg);


#endif
