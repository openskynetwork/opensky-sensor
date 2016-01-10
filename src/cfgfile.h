/* Copyright (c) 2015-2016 Sero Systems <contact at sero-systems dot de> */

#ifndef _HAVE_CFGFILE_H
#define _HAVE_CFGFILE_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <netdb.h>

struct CFG_WD {
	bool enabled;
};

struct CFG_FPGA {
	bool configure;
	char file[PATH_MAX];
	uint32_t retries;
	uint32_t timeout;
};

struct CFG_INPUT {
	union {
		struct {
			char uart[PATH_MAX];
			bool rtscts;
		};
		struct {
			char host[NI_MAXHOST];
			uint16_t port;
		};
	};

	uint32_t reconnectInterval;
};

struct CFG_RECV {
	bool modeSLongExtSquitter;
	bool syncFilter;
	bool crc;
	bool fec;
	bool gps;
};

struct CFG_NET {
	char host[NI_MAXHOST];
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

struct CFG_STATS {
	bool enabled;
	uint32_t interval;
};

struct CFG_GPS {
	union {
		char uart[PATH_MAX];
		struct {
			char host[NI_MAXHOST];
			uint16_t port;
		};
	};

	uint32_t reconnectInterval;
};

struct CFG_Config {
	struct CFG_WD wd;
	struct CFG_FPGA fpga;
	struct CFG_INPUT input;
	struct CFG_RECV recv;
	struct CFG_NET net;
	struct CFG_BUF buf;
	struct CFG_DEV dev;
	struct CFG_STATS stats;
	struct CFG_GPS gps;
};

extern struct CFG_Config CFG_config;

void CFG_read(const char * file);

#endif
