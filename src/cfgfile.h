/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_CFGFILE_H
#define _HAVE_CFGFILE_H

#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

enum CFG_VALUE_TYPE {
	CFG_VALUE_TYPE_STRING,
	CFG_VALUE_TYPE_INT,
	CFG_VALUE_TYPE_BOOL,
};

union CFG_Value {
	char * string;
	uint32_t integer;
	bool boolean;
};

struct CFG_Option {
	const char * name;
	enum CFG_VALUE_TYPE type;
	union CFG_Value * var;
	union CFG_Value def;
};

struct CFG_Section {
	const char * name;
	void (*check)(struct CFG_Section *);
	int n_opt;
	struct CFG_Option options[];
};

bool CFG_registerSection(const struct CFG_Section * section);
void CFG_parse(const char * file);


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
	char deviceName[20];
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

void CFG_loadDefaults();
bool CFG_readFile(const char * file);
bool CFG_check();

#ifdef __cplusplus
}
#endif

#endif
