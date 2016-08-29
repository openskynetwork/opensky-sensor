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

static size_t n_sections;
static struct CFG_Section const ** sections;

/** Current begin of parser */
static const char * bufferInput;
/** Remaining size of the file to be parsed */
static off_t bufferSize;
/** Current line number */
static uint_fast32_t bufferLine;


static void loadDefaults();
static void readCfg(const char * cfgStr, off_t size);


bool CFG_registerSection(const struct CFG_Section * section)
{
	struct CFG_Section const ** newSections =
		realloc(sections, (n_sections + 1) * sizeof(sections));
	if (newSections == NULL)
		return false;
	sections[n_sections++] = section;
	return true;
}

/** Load default configuration values.
 * \note this should be called prior to CFG_readFile()
 */
void CFG_loadDefaults()
{
	loadDefaults();
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
	//fix(&CFG_config);

	return true;
}

/** Check the current configuration.
 * \note Should be called after CFG_readFile()
 * \return true if configuration is correct, false otherwise.
 */
bool CFG_check()
{
	/* check configuration */
	//return check(&CFG_config);
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
static const struct CFG_Section * parseSection()
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
		return NULL;
	}

	return sections[section];
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

#if 0
/** Parse an integer for port. Garbage after the number is ignored.
 * \param opt option to be parsed
 * \param ret parsed port. Only written if parsing was successful
 * \return true if parsing was successful, false otherwise
 */
static inline bool parsePort(const char * value, size_t valLen, uint32_t * ret)
{
	uint32_t n;
	if (parseInt(opt, &n)) {
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
#endif

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
		return parseString(value, valLen, option->var->string, 20); /* TODO */
		break;
	case CFG_VALUE_TYPE_INT:
		return parseInt(value, valLen, &option->var->integer);
		break;
	case CFG_VALUE_TYPE_BOOL:
		return parseBool(value, valLen, &option->var->boolean);
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
static bool parseOption(const struct CFG_Section * section)
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

	if (section) {
		uint32_t i;
		for (i = 0; i < section->n_opt; ++i) {
			if (isSame(section->options[i].name, key, keyLen)) {
				assignOptionFromString(&section->options[i], value, valLen);
				break;
			}
		}
		if (i > section->n_opt)
			unknownKey(key, keyLen);
	}

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
	bool noSection = true;
	const struct CFG_Section * section = NULL;

	/* initialize parser buffer */
	bufferInput = cfgStr;
	bufferSize = size;
	bufferLine = 1;

	while (bufferSize) {
		switch (bufferInput[0]) {
		case '[':
			section = parseSection();
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
			if (noSection)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": "
					"Unexpected option outside any section", bufferLine);
			parseOption(section);
		}
	}
}

/** Load default values.
 * \param cfg configuration
 */
static void loadDefaults()
{
	size_t sect;

	for (sect = 0; sect < n_sections; ++sect) {
		size_t i;
		for (i = 0; i < sections[sect]->n_opt; ++i) {
			const struct CFG_Option * opt = &sections[sect]->options[i];
			switch (opt->type) {
			case CFG_VALUE_TYPE_STRING:
				opt->var->string = opt->def.string;
			}
		}
	}
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

