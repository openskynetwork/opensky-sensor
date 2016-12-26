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

/** Value Type Description */
enum CFG_VALUE_TYPE {
	/** String type */
	CFG_VALUE_TYPE_STRING,
	/** Integer type (unsigned, 32 bit) */
	CFG_VALUE_TYPE_INT,
	/** Port type (unsigned, 16 bit) */
	CFG_VALUE_TYPE_PORT,
	/** Boolean type */
	CFG_VALUE_TYPE_BOOL,
	/** IEEE 754 double precision floating point type */
	CFG_VALUE_TYPE_DOUBLE,
};

/** Value, discriminate by type */
union CFG_Value {
	/** Pointer to string */
	char * string;
	/** Integer */
	uint32_t integer;
	/** Port */
	uint_fast16_t port;
	/** Boolean */
	bool boolean;
	/** Double precision floating point */
	double dbl;
};

/** Configuration option */
struct CFG_Option {
	/** Option name */
	const char * name;
	/** Option value type */
	enum CFG_VALUE_TYPE type;
	/** Pointer to option value */
	void * var;
	/** (Optional) Pointer to a boolean, telling whether the option had been
	 * set */
	bool * given;
	/** For strings: buffer length */
	size_t maxlen;
	/** Default value */
	union CFG_Value def;
};

/** Configuration section */
struct CFG_Section {
	/** Section name */
	const char * name;
	/** (Optional) Callback for fixing the configuration
	 * @param section pointer to this section
	 */
	void (*fix)(const struct CFG_Section * section);
	/** (Optional) Callback for checking the configuration
	 * @param pointer to this section
	 * @return true if check was successful
	 */
	bool (*check)(const struct CFG_Section * section);
	/** Number of options in this section */
	int n_opt;
	/** Variadic array of options */
	struct CFG_Option options[];
};

void CFG_registerSection(const struct CFG_Section * section);
void CFG_unregisterAll();

void CFG_setOptions(bool warnUnknownSection, bool warnUnknownOption,
	bool onErrorUseDefault);

void CFG_loadDefaults();
bool CFG_readFile(const char * file, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault);
bool CFG_write(const char * filename);
bool CFG_writeSection(const char * filename,
	const struct CFG_Section * section);
bool CFG_readDirectory(const char * path, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault,
	bool stopOnFirstError);
bool CFG_check();

void CFG_setBoolean(const char * section, const char * key, bool value);
void CFG_setInteger(const char * section, const char * key, uint32_t value);
void CFG_setPort(const char * section, const char * key, uint_fast16_t value);
void CFG_setString(const char * section, const char * key, const char * value);
void CFG_setDouble(const char * section, const char * key, double value);

#ifdef __cplusplus
}
#endif

#endif
