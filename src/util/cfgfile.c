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
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "log.h"
#include "cfgfile.h"
#include "util.h"

static const char PFX[] = "CFG";

//#define DEBUG

static size_t n_sections = 0;
static struct CFG_Section const ** sections = NULL;

/** silent options */
static bool optWarnUnknownSection = true;
static bool optWarnUnknownOption = true;
static bool optOnErrorUseDefault = true;

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
	SECTION_STATE_UNRECOVERABLE
};

static const char * fileName;
static enum SECTION_STATE sectionState;
static const char * sectionName;
static size_t sectionNameLen;

static bool readCfg();
static void fix();
static void assignOptionFromDefault(const struct CFG_Option * opt);

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
			[CFG_VALUE_TYPE_DOUBLE] = "double",
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
bool CFG_readFile(const char * file, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault)
{
	optWarnUnknownSection = warnUnknownSection;
	optWarnUnknownOption = warnUnknownOption;
	optOnErrorUseDefault = onErrorUseDefault;

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
	char * buffer = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close(fd);
	if (buffer == MAP_FAILED) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not mmap '%s'", file);
		return false;
	}

	bufferInput = buffer;
	bufferSize = st.st_size;
	bufferLine = 0;
	fileName = file;

	/* actually read configuration */
	bool success = readCfg();

	/* unmap file */
	munmap(buffer, st.st_size);

	/* fix configuration */
	if (success)
		fix();

	return success;
}

static int filterDirectory(const struct dirent * ent)
{
	if (ent->d_type != DT_REG)
		return 0;
	char first = ent->d_name[0];
	if (first < '0' || first > '9')
		return 0;
	size_t len = strlen(ent->d_name);
	if (len < 4)
		return 0;
	if (strcmp(ent->d_name + len - 4, ".conf"))
		return 0;
	return 1;
}

static int compareDirectory(const struct dirent ** a, const struct dirent ** b)
{
	unsigned long int idx_a = strtoul((*a)->d_name, NULL, 10);
	unsigned long int idx_b = strtoul((*b)->d_name, NULL, 10);
	return idx_a < idx_b ? -1 : idx_a == idx_b ? 0 : 1;
}

bool CFG_readDirectory(const char * path, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault,
	bool stopOnFirstError)
{
	struct dirent ** namelist;
	int s = scandir(path, &namelist, &filterDirectory, &compareDirectory);
	if (s < 0) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not traverse path '%s'", path);
		return false;
	}

	char filename[PATH_MAX];
	size_t len = strlen(path);
	if (len > PATH_MAX - 1) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Path '%s' too long", path);
		return false;
	}
	strncpy(filename, path, sizeof filename);
	filename[len] = '/';
	char * foffset = filename + len + 1;
	len = sizeof filename - (len + 1);
	bool ret = false;
	int n;
	for (n = 0; n < s; ++n) {
		const char * fname = namelist[n]->d_name;
		size_t flen = strlen(namelist[n]->d_name);
		if (flen > len) {
			LOG_logf(LOG_LEVEL_ERROR, PFX, "Path '%s/%s' is too long", path,
				fname);
			if (stopOnFirstError)
				goto ret;
			continue;
		}
		strncpy(foffset, fname, len);
		if (!CFG_readFile(filename, warnUnknownSection, warnUnknownOption,
			onErrorUseDefault) && stopOnFirstError)
			goto ret;
	}
	ret = true;

ret:
	for (n = 0; n < s; ++n)
		free(namelist[n]);
	free(namelist);

	return ret;
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

__attribute__((format(printf, 2, 3)))
static void err(enum LOG_LEVEL logLevel, const char * fmt, ...) {
	char buf[512];

	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	LOG_logf(logLevel, PFX, "FILE '%s', Line %" PRIuFAST32 ": %s",
		fileName, bufferLine, buf);
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
	if (!c || n < c) {
		err(LOG_LEVEL_ERROR, "] expected before end of line");
		return SECTION_STATE_UNRECOVERABLE;
	}
	if (c != n - 1) {
		err(LOG_LEVEL_ERROR, "newline after ] expected");
		return SECTION_STATE_UNRECOVERABLE;
	}

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
		if (optWarnUnknownSection)
			err(LOG_LEVEL_WARN, "Section %.*s is unknown", (int)len, buf);
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
		err(LOG_LEVEL_ERROR, "Number expected");
		return false;
	}
	strncpy(buf, value, valLen);
	buf[valLen] = '\0';

	char * end;
	unsigned long int n = strtoul(buf, &end, 0);
	if (*end != '\0')
		err(LOG_LEVEL_WARN, "Garbage after number ignored");
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
			err(LOG_LEVEL_ERROR, "Port must be < 65536");
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

	err(LOG_LEVEL_ERROR, "Boolean option has unexpected value '%.*s'",
		(int)valLen, value);
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
		err(LOG_LEVEL_ERROR, "Value too long (max. length %zu expected)",
			sz - 1);
		return false;
	}
	memcpy(str, value, valLen);
	str[valLen] = '\0';
	return true;
}

/** Parse a double. Garbage after the number is ignored.
 * \param opt option to be parsed
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseDouble(const char * value, size_t valLen, double * ret)
{
	char buf[60];
	if (valLen + 1 > sizeof buf || valLen == 0) {
		err(LOG_LEVEL_ERROR, "Number expected");
		return false;
	}
	strncpy(buf, value, valLen);
	buf[valLen] = '\0';

	char * end;
	double d = strtod(buf, &end);
	if (*end != '\0')
		err(LOG_LEVEL_WARN, "Garbage after number ignored");
	*ret = d;

	return true;
}

/** Stop on unknown key.
 * \param opt option which couldn't be recognized
 */
static inline void unknownKey(const char * key, size_t keyLen)
{
	err(LOG_LEVEL_WARN, "Unknown key '%.*s'", (int)keyLen, key);
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
	case CFG_VALUE_TYPE_DOUBLE:
		return parseDouble(value, valLen, option->var);
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

	const char * key = bufferInput;

	/* advance buffer */
	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;

	/* sanity check */
	if (unlikely(n < e)) {
		err(LOG_LEVEL_ERROR, "'=' expected before end of line");
		++bufferLine;
		return false;
	}

	if (sectionState == SECTION_STATE_VALID) {
		/* eliminate whitespaces at the end of the key */
		const char * l = e - 1;
		while (l > key && isspace(*l))
			--l;
		size_t keyLen = l + 1 - key;

		/* eliminate whitespaces at the beginning of the value */
		const char * value = e + 1;
		while (value < n && isspace(*value))
			++value;
		size_t valLen = n - value;

		size_t sectidx;
		for (sectidx = 0; sectidx < n_sections; ++sectidx) {
			const struct CFG_Section * section = sections[sectidx];
			if (isSame(section->name, sectionName, sectionNameLen)) {
				size_t i;
				for (i = 0; i < section->n_opt; ++i) {
					if (isSame(section->options[i].name, key, keyLen)) {
						const struct CFG_Option * opt = &section->options[i];
						if (!assignOptionFromString(opt, value, valLen)) {
							if (optOnErrorUseDefault)
								assignOptionFromDefault(opt);
							else
								return false;
						} else if (opt->given) {
							*opt->given = true;
						}
						goto found;
					}
				}
			}
		}
		if (sectidx >= n_sections && optWarnUnknownOption)
			unknownKey(key, keyLen);
	}

found:
	++bufferLine;

	return true;
}

/** Read the configuration file.
 * \param cfgStr configuration, loaded into memory
 * \param size size of the configuration
 * \param cfg configuration structure to be filled
 */
static bool readCfg()
{
	sectionState = SECTION_STATE_UNKNOWN;

	while (bufferSize) {
		switch (bufferInput[0]) {
		case '[':
			sectionState = parseSection();
			if (sectionState == SECTION_STATE_UNRECOVERABLE)
				return false;
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
			if (sectionState == SECTION_STATE_NOSECTION && optWarnUnknownOption)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": "
					"Unexpected option outside any section", bufferLine);
			parseOption();
		}
	}
	return true;
}

static void assignOptionFromDefault(const struct CFG_Option * opt)
{
	union CFG_Value * val = opt->var;
	switch (opt->type) {
	case CFG_VALUE_TYPE_BOOL:
		val->boolean = opt->def.boolean;
		break;
	case CFG_VALUE_TYPE_INT:
		val->integer = opt->def.integer;
		break;
	case CFG_VALUE_TYPE_PORT:
		val->port = opt->def.port;
		break;
	case CFG_VALUE_TYPE_STRING:
		if (opt->def.string)
			strncpy(opt->var, opt->def.string, opt->maxlen);
		else
			((char*)opt->var)[0] = '\0';
		break;
	case CFG_VALUE_TYPE_DOUBLE:
		val->dbl = opt->def.dbl;
		break;
	}
	if (opt->given)
		*opt->given = false;
}

/** Load default values. */
void CFG_loadDefaults()
{
	size_t sect, i;

	for (sect = 0; sect < n_sections; ++sect)
		for (i = 0; i < sections[sect]->n_opt; ++i)
			assignOptionFromDefault(&sections[sect]->options[i]);
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

void CFG_setDouble(const char * section, const char * option, double value)
{
	const struct CFG_Option * opt = getOption(section, option);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_DOUBLE);
		((union CFG_Value*)opt->var)->dbl = value;
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
		case CFG_VALUE_TYPE_DOUBLE:
			fprintf(file, "%f\n", val->dbl);
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

void CFG_writeSection(const char * filename, const struct CFG_Section * section)
{
	FILE * file = fopen(filename, "w");
	if (!file) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not write configuration file "
			"'%s'", filename);
		return;
	}

	fprintf(file, "[%s]\n", section->name);
	writeOptions(file, section);

	fclose(file);
}
