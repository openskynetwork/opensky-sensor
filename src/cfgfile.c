/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "log.h"
#include "cfgfile.h"
#include "util.h"

static const char PFX[] = "CFG";

//#define DEBUG

static size_t n_sections;
static struct CFG_Section const ** sections;

/** Current begin of parser */
static const char * bufferInput;
/** Remaining size of the file to be parsed */
static off_t bufferSize;
/** Current line number */
static uint_fast32_t bufferLine;

enum SECTION_STATE {
	SECTION_STATE_NOSECTION,
	SECTION_STATE_UNKNOWN,
	SECTION_STATE_VALID,
};

static enum SECTION_STATE sectionState;
static const char * sectionName;
static size_t sectionNameLen;

static void readCfg(const char * cfgStr, off_t size);
static void fix();

void CFG_registerSection(const struct CFG_Section * section)
{
	struct CFG_Section const ** newSections =
		realloc(sections, (n_sections + 1) * sizeof(sections));
	if (newSections == NULL)
		LOG_errno(LOG_LEVEL_EMERG, PFX, "malloc failed");
	sections = newSections;
#ifdef DEBUG
	LOG_logf(LOG_LEVEL_INFO, PFX, "Registered Section %s with %d options",
		section->name, section->n_opt);
	size_t i;
	for (i = 0; i < section->n_opt; ++i) {
		const struct CFG_Option * opt = &section->options[i];
		static const char * types[] = {
			[CFG_VALUE_TYPE_BOOL] = "bool",
			[CFG_VALUE_TYPE_INT] = "int",
			[CFG_VALUE_TYPE_STRING] = "string",
			[CFG_VALUE_TYPE_PORT] = "port",
		};
		LOG_logf(LOG_LEVEL_INFO, PFX, " - %s: %s", opt->name, types[opt->type]);
	}
#endif
	sections[n_sections++] = section;
}

void CFG_unregisterAll()
{
	free(sections);
	sections = NULL;
	n_sections = 0;
}

/** Read a configuration file.
 * \param file configuration file name to be read
 * \return true if reading succeeded, false otherwise
 */
bool CFG_readFile(const char * file)
{
	/* open input file */
	int fd = open(file, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not open '%s'", file);
		return false;
	}

	/* stat input file for its size */
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not stat '%s'", file);
		return false;
	}

	/* mmap input file */
	char * cfgStr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (cfgStr == MAP_FAILED) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not mmap '%s'", file);
		return false;
	}

	/* actually read configuration */
	readCfg(cfgStr, st.st_size);

	/* unmap file */
	munmap(cfgStr, st.st_size);

	/* fix configuration */
	fix();

	return true;
}

/** Checks two strings for equality (neglecting the case) where one string is
 * not NUL-terminated.
 * \param str1 NUL-terminated string to compare
 * \param str2 non NUL-terminated string to compare with
 * \param str2len length of second string
 * \return true if strings are equal
 */
static bool isSame(const char * str1, const char * str2, size_t str2len)
{
	size_t str1len = strlen(str1);
	return str1len == str2len && !strncasecmp(str2, str1, str2len);
}

/** Parse a section name.
 * \return section
 */
static enum SECTION_STATE parseSection()
{
	/* get next ']' terminator */
	const char * c = memchr(bufferInput, ']', bufferSize);
	/* get next newline or end-of-file */
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;

	/* sanity checks */
	if (!c || n < c)
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": ] expected "
			"before end of line", bufferLine);
	if (c != n - 1)
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": newline after ] "
			"expected", bufferLine);

	/* calculate length */
	const ptrdiff_t len = c - bufferInput - 1;
	const char * const buf = bufferInput + 1;

	/* search section */
	size_t section;
	for (section = 0; section < n_sections; ++section)
		if (isSame(sections[section]->name, buf, len))
			break;

	/* advance parser and return */
	bufferInput += len + 3;
	bufferSize -= len + 3;
	++bufferLine;

	if (section > n_sections) {
		LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": Section %.*s is "
			"unknown", bufferLine, (int)len, buf);
		return SECTION_STATE_UNKNOWN;
	}

	sectionName = buf;
	sectionNameLen = len;

	return SECTION_STATE_VALID;
}

/** Scan a comment. This will just advance the parser to the end of line/file */
static void skipComment()
{
	/* search for next newline */
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	/* advance parser */
	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;
	++bufferLine;
}

/** Parse an integer. Garbage after the number is ignored.
 * \param opt option to be parsed
 * \param ret parsed integer. Only written if parsing was successful
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseInt(const char * value, size_t valLen, uint32_t * ret)
{
	char buf[20];
	if (valLen + 1 > sizeof buf || valLen == 0) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": Number expected",
			bufferLine);
		return false;
	}
	strncpy(buf, value, valLen);
	buf[valLen] = '\0';

	char * end;
	unsigned long int n = strtoul(buf, &end, 0);
	if (*end != '\0') {
		LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": Garbage after "
			"number ignored", bufferLine);

	}
	*ret = n;

	return true;
}

/** Parse an integer for port. Garbage after the number is ignored.
 * \param opt option to be parsed
 * \param ret parsed port. Only written if parsing was successful
 * \return true if parsing was successful, false otherwise
 */
static inline bool parsePort(const char * value, size_t valLen,
	uint_fast16_t * ret)
{
	uint32_t n;
	if (parseInt(value, valLen, &n)) {
		if (n > 0xffff) {
			LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32
				": port must be < 65536", bufferLine);
		} else {
			*ret = n;
			return true;
		}
	}
	return false;
}

/** Parse a boolean.
 * \param opt option to be parsed
 * \param ret parsed boolean. Only written if parsing was successful
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseBool(const char * value, size_t valLen, bool * ret)
{
	if (isSame("true", value, valLen) || isSame("1", value, valLen)) {
		*ret = true;
		return true;
	}
	if (isSame("false", value, valLen) || isSame("0", value, valLen)) {
		*ret = false;
		return true;
	}

	LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": boolean option has "
		"unexpected value '%.*s'", bufferLine, (int)valLen, value);
	return false;
}

/** Parse a string.
 * \param opt option to be parsed
 * \param str string to read into (will be nul-terminated), only written if
 *  parsing was successful.
 * \param sz size of the string
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseString(const char * value, size_t valLen, char * str,
	size_t sz)
{
	if (valLen > sz - 1) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": Value too long "
			"(max. length %zu expected)", bufferLine, sz - 1);
		return false;
	}
	memcpy(str, value, valLen);
	str[valLen] = '\0';
	return true;
}

/** Stop on unknown key.
 * \param opt option which couldn't be recognized
 */
static inline void unknownKey(const char * key, size_t keyLen)
{
	LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": unknown key '%.*s'",
		bufferLine, (int)keyLen, key);
}

static bool assignOptionFromString(const struct CFG_Option * option,
	const char * value, size_t valLen)
{
	switch (option->type) {
	case CFG_VALUE_TYPE_STRING:
		return parseString(value, valLen, option->var, option->maxlen);
		break;
	case CFG_VALUE_TYPE_INT:
		return parseInt(value, valLen, option->var);
		break;
	case CFG_VALUE_TYPE_BOOL:
		return parseBool(value, valLen, option->var);
		break;
	case CFG_VALUE_TYPE_PORT:
		return parsePort(value, valLen, option->var);
		break;
	default:
		assert(false);
		return false;
	}
}

/** Parse an option line.
 * \param section current section
 * \param cfg configuration to be filled
 */
static bool parseOption()
{
	/* split option on '='-sign */
	const char * e = memchr(bufferInput, '=', bufferSize);
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	/* sanity check */
	if (unlikely(n < e)) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": '=' before end "
			"of line expected", bufferLine);
		return false;
	}

	/* eliminate whitespaces at the end of the key */
	const char * l = e - 1;
	while (l > bufferInput && isspace(*l))
		--l;
	const char * key = bufferInput;
	size_t keyLen = l + 1 - key;

	/* eliminate whitespaces at the beginning of the value */
	const char * value = e + 1;
	while (value < n && isspace(*value))
		++value;
	size_t valLen = n - value;

	if (sectionState == SECTION_STATE_VALID) {
		size_t sectidx;
		for (sectidx = 0; sectidx < n_sections; ++sectidx) {
			const struct CFG_Section * section = sections[sectidx];
			if (isSame(section->name, sectionName, sectionNameLen)) {
				size_t i;
				for (i = 0; i < section->n_opt; ++i) {
					if (isSame(section->options[i].name, key, keyLen)) {
						assignOptionFromString(&section->options[i], value,
							valLen);
						goto found;
					}
				}
			}
		}
		if (sectidx >= n_sections)
			unknownKey(key, keyLen);
	}

found:
	/* advance buffer */
	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;
	++bufferLine;

	return true;
}

/** Read the configuration file.
 * \param cfgStr configuration, loaded into memory
 * \param size size of the configuration
 * \param cfg configuration structure to be filled
 */
static void readCfg(const char * cfgStr, off_t size)
{
	sectionState = SECTION_STATE_UNKNOWN;

	/* initialize parser buffer */
	bufferInput = cfgStr;
	bufferSize = size;
	bufferLine = 1;

	while (bufferSize) {
		switch (bufferInput[0]) {
		case '[':
			sectionState = parseSection();
		break;
		case '\n':
			++bufferInput;
			--bufferSize;
			++bufferLine;
		break;
		case ';':
		case '#':
			skipComment();
		break;
		default:
			if (sectionState == SECTION_STATE_NOSECTION)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": "
					"Unexpected option outside any section", bufferLine);
			parseOption();
		}
	}
}

/** Load default values. */
void CFG_loadDefaults()
{
	size_t sect;

	for (sect = 0; sect < n_sections; ++sect) {
		size_t i;
		for (i = 0; i < sections[sect]->n_opt; ++i) {
			const struct CFG_Option * opt = &sections[sect]->options[i];
			union CFG_Value * val = opt->var;
			switch (opt->type) {
			case CFG_VALUE_TYPE_BOOL:
#ifdef DEBUG
				LOG_logf(LOG_LEVEL_INFO, PFX, "bool %s.%s = %s",
					sections[sect]->name, opt->name, opt->def.boolean ? "true" :
						"false");
#endif
				val->boolean = opt->def.boolean;
				break;
			case CFG_VALUE_TYPE_INT:
#ifdef DEBUG
				LOG_logf(LOG_LEVEL_INFO, PFX, "int %s.%s = %" PRIu32,
					sections[sect]->name, opt->name, opt->def.integer);
#endif
				val->integer = opt->def.integer;
				break;
			case CFG_VALUE_TYPE_PORT:
#ifdef DEBUG
				LOG_logf(LOG_LEVEL_INFO, PFX, "port %s.%s = %" PRIuFAST16,
					sections[sect]->name, opt->name, opt->def.port);
#endif
				val->port = opt->def.port;
				break;
			case CFG_VALUE_TYPE_STRING:
#ifdef DEBUG
				if (opt->def.string)
					LOG_logf(LOG_LEVEL_INFO, PFX, "string %s.%s = \"%s\"",
						sections[sect]->name, opt->name, opt->def.string);
				else
					LOG_logf(LOG_LEVEL_INFO, PFX, "string %s.%s = NULL",
						sections[sect]->name, opt->name);
#endif
				if (opt->def.string)
					strncpy(opt->var, opt->def.string, opt->maxlen);
				else
					((char*)opt->var)[0] = '\0';
				break;
			}
		}
	}
}

static const struct CFG_Option * getOption(const char * section,
	const char * option)
{
	size_t sect;

	for (sect = 0; sect < n_sections; ++sect) {
		if (strcasecmp(sections[sect]->name, section))
			continue;
		size_t i;
		for (i = 0; i < sections[sect]->n_opt; ++i) {
			const struct CFG_Option * opt = &sections[sect]->options[i];
			if (!strcasecmp(opt->name, option))
				return opt;
		}
	}
	LOG_logf(LOG_LEVEL_EMERG, PFX, "Could not find option %s.%s\n", section,
		option);
	return false;
}

void CFG_setBoolean(const char * section, const char * option, bool value)
{
	const struct CFG_Option * opt = getOption(section, option);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_BOOL);
		((union CFG_Value*)opt->var)->boolean = value;
	}
}

void CFG_setInteger(const char * section, const char * option, uint32_t value)
{
	const struct CFG_Option * opt = getOption(section, option);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_INT);
		((union CFG_Value*)opt->var)->integer = value;
	}
}

void CFG_setPort(const char * section, const char * option,
	uint_fast16_t value)
{
	const struct CFG_Option * opt = getOption(section, option);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_PORT);
		((union CFG_Value*)opt->var)->port = value;
	}
}

void CFG_setString(const char * section, const char * option,
	const char * value)
{
	const struct CFG_Option * opt = getOption(section, option);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_STRING);
		strncpy(opt->var, value, opt->maxlen);
	}
}

static void fix()
{
	size_t sect;

	for (sect = 0; sect < n_sections; ++sect) {
		const struct CFG_Section * section = sections[sect];
		if (section->fix)
			section->fix(section);
	}
}

/** Check the current configuration.
 * \note Should be called after CFG_readFile()
 * \return true if configuration is correct, false otherwise.
 */
bool CFG_check()
{
	size_t sect;

	for (sect = 0; sect < n_sections; ++sect) {
		const struct CFG_Section * section = sections[sect];
		if (section->check && !section->check(section))
			return false;
	}
	return true;
}

static void writeOptions(FILE * file, const struct CFG_Section * section)
{
	size_t n;
	for (n = 0; n < section->n_opt; ++n) {
		const struct CFG_Option * opt = &section->options[n];
		const union CFG_Value * val = opt->var;
		fprintf(file, "%s = ", opt->name);
		switch (opt->type) {
		case CFG_VALUE_TYPE_BOOL:
			fprintf(file, "%s\n", val->boolean ? "true" : "false");
			break;
		case CFG_VALUE_TYPE_INT:
			fprintf(file, "%" PRIu32 "\n", val->integer);
			break;
		case CFG_VALUE_TYPE_PORT:
			fprintf(file, "%" PRIuFAST16 "\n", val->port);
			break;
		case CFG_VALUE_TYPE_STRING:
			fprintf(file, "%s\n", (const char*)opt->var);
			break;
		}
	}
}

void CFG_write(const char * filename)
{
	FILE * file = fopen(filename, "w");
	if (!file) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not write configuration file "
			"'%s'", filename);
		return;
	}

	bool first = true;
	size_t s;
	for (s = 0; s < n_sections; ++s) {
		const struct CFG_Section * sect = sections[s];
		size_t prev;
		for (prev = 0; prev < s; ++prev)
			if (!strcasecmp(sect->name, sections[prev]->name))
				break;
		if (prev < s)
			continue;
		if (!first)
			fprintf(file, "\n\n");
		first = false;
		fprintf(file, "[%s]\n", sect->name);
		writeOptions(file, sect);

		size_t next;
		for (next = s + 1; next < n_sections; ++next)
			if (!strcasecmp(sect->name, sections[next]->name))
				writeOptions(file, sections[next]);
	}
	fclose(file);
}

#if 0

static void fix(struct CFG_Config * cfg)
{
#ifndef STANDALONE
	if (cfg->fpga.configure) {
		cfg->fpga.configure = false;
		LOG_log(LOG_LEVEL_WARN, PFX, "FPGA.configure is ignored in"
			"non-standalone mode");
	}

	if (cfg->wd.enabled) {
		cfg->wd.enabled = false;
		LOG_log(LOG_LEVEL_WARN, PFX, "WD.enabled is ignored in non-standalone "
			"mode");
	}
#endif

	if (cfg->buf.statBacklog < 2) {
		cfg->buf.statBacklog = 2;
		LOG_log(LOG_LEVEL_WARN, PFX, "BUFFER.staticBacklog was increased to 2");
	}

	if (cfg->buf.gcEnabled && !cfg->buf.history) {
		cfg->buf.gcEnabled = false;
		LOG_log(LOG_LEVEL_WARN, PFX, "Ignoring BUFFER.gcEnabled because "
			"BUFFER.history is not enabled");

	}

	if (cfg->dev.serialSet) {
		if (cfg->dev.serial & 0x80000000) {
			cfg->dev.serial &= 0x7fffffff;
			LOG_logf(LOG_LEVEL_WARN, PFX, "DEVICE.serial was truncated to 31 "
				"bits, it is %" PRIu32 " now", cfg->dev.serial);
		}
	} else if (cfg->dev.deviceName[0]) {
		cfg->dev.serialSet = UTIL_getSerial(cfg->dev.deviceName,
			&cfg->dev.serial);
	}
}

/** Check configuration for sanity.
 * \param cfg configuration
 */
static bool check(const struct CFG_Config * cfg)
{
	if (cfg->fpga.configure && cfg->fpga.file[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "FPGA.file is missing");
		return false;
	}

	if (cfg->buf.statBacklog <= 2) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "BUFFER.staticBacklog must be >= 2");
		return false;
	}

	if (cfg->net.host[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.host is missing");
		return false;
	}
	if (cfg->net.port == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "NET.port = 0");
		return false;
	}

#ifdef INPUT_RADARCAPE_NETWORK
	if (cfg->input.host[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.host is missing");
		return false;
	}
	if (cfg->input.port == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.port = 0");
		return false;
	}
#elif defined(INPUT_RADARCAPE_UART)
	if (cfg->input.uart[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.uart is missing");
		return false;
	}
#endif

	if (!cfg->dev.serialSet) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "DEVICE.serial is missing");
		return false;
	}

	if (cfg->stats.interval == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "STATISTICS.interval = 0");
		return false;
	}

#ifdef INPUT_RADARCAPE_NETWORK
	if (cfg->gps.host[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.host is missing");
		return false;
	}
	if (cfg->gps.port == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.port = 0");
		return false;
	}
#elif defined(INPUT_RADARCAPE_UART)
	if (cfg->gps.uart[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.uart is missing");
		return false;
	}
#endif

	return true;
}
#endif

