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
	CFG_VALUE_TYPE_PORT,
	CFG_VALUE_TYPE_BOOL,
};

union CFG_Value {
	char * string;
	uint32_t integer;
	uint_fast16_t port;
	bool boolean;
};

struct CFG_Option {
	const char * name;
	enum CFG_VALUE_TYPE type;
	void * var;
	size_t maxlen;
	union CFG_Value def;
};

struct CFG_Section {
	const char * name;
	void (*fix)(const struct CFG_Section *);
	bool (*check)(const struct CFG_Section *);
	int n_opt;
	struct CFG_Option options[];
};

void CFG_registerSection(const struct CFG_Section * section);
void CFG_unregisterAll();

void CFG_setOptions(bool warnUnknownSection, bool warnUnknownOption,
	bool onErrorUseDefault);

void CFG_loadDefaults();
bool CFG_readFile(const char * file, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault);
void CFG_write(const char * filename);
bool CFG_readDirectory(const char * path, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault,
	bool stopOnFirstError);
bool CFG_check();

void CFG_setBoolean(const char * section, const char * option, bool value);
void CFG_setInteger(const char * section, const char * option, uint32_t value);
void CFG_setPort(const char * section, const char * option,
	uint_fast16_t value);
void CFG_setString(const char * section, const char * option,
	const char * value);

#ifdef __cplusplus
}
#endif

#endif
