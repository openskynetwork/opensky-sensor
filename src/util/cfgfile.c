/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
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
#include "list.h"
#include "port/misc.h"
#include "port/mmap.h"

/** Component: Prefix */
static const char PFX[] = "CFG";

/* Define to 1 to enable debugging messages */
//#define DEBUG 1

/** Internal section: section and link to siblings */
struct Section {
	/** pointer to section */
	struct CFG_Section const * section;
	/** siblings in list */
	struct LIST_LinkD list;
};

/** List of sections */
static struct LIST_ListD sections = LIST_ListD_INIT(sections);

/** Parser option: warn if sections is unknown */
static bool optWarnUnknownSection = true;
/** Parser option: warn if option is unknown */
static bool optWarnUnknownOption = true;
/** Parser option: if option value cannot be parsed, use default value */
static bool optOnErrorUseDefault = true;

/** Parser state: section state */
enum SECTION_STATE {
	/** Not in a section */
	SECTION_STATE_NOSECTION,
	/** Unknown section */
	SECTION_STATE_UNKNOWN,
	/** Valid section */
	SECTION_STATE_VALID,
	/** Unrecoverable error */
	SECTION_STATE_UNRECOVERABLE
};

/** Parser state: current begin*/
static const char * bufferInput;
/** Parser state: remaining size of the file to be parsed */
static off_t bufferSize;
/** Parser state: current line number */
static uint_fast32_t bufferLine;
/* Parser state: filename */
static const char * fileName;
/** Parser state: section state */
static enum SECTION_STATE sectionState;
/** Parser state: section name */
static const char * sectionName;
/** Parser state: section name length */
static size_t sectionNameLen;

static bool readCfg();
static void fix();
static void assignOptionFromDefault(const struct CFG_Option * opt);

/** Register a new configuration section.
 * @param section section to be registered
 */
void CFG_registerSection(const struct CFG_Section * section)
{
	struct Section * s = malloc(sizeof *s);
	if (s == NULL)
		LOG_errno(LOG_LEVEL_EMERG, PFX, "malloc failed");

	s->section = section;
	LIST_push(&sections, &s->list);
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
}

/** Unregister all sections */
void CFG_unregisterAll()
{
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&sections, l, n)
		free(LIST_item(l, struct Section, list));
	LIST_init(&sections);
}

/** Read a configuration file.
 * @param file configuration filename to be read
 * @param warnUnknownSection warn if section is unknown
 * @param warnUnknownOption warn if options is unknown
 * @param onErrorUseDefault if option value cannot be parsed, use default value
 *  Otherwise, parsing will fail
 * @return true if reading succeeded, false otherwise
 */
bool CFG_readFile(const char * file, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault)
{
	optWarnUnknownSection = warnUnknownSection;
	optWarnUnknownOption = warnUnknownOption;
	optOnErrorUseDefault = onErrorUseDefault;

	struct MMAP * map = MMAP_open(PFX, file);
	if (map == NULL)
		return false;

	/* initialize parser */
	bufferInput = MMAP_getPtr(map);
	bufferSize = MMAP_getSize(map);
	bufferLine = 1;
	fileName = file;

	/* actually read configuration */
	bool success = readCfg();

	MMAP_close(map);

	/* fix configuration */
	if (success)
		fix();

	return success;
}

/** Filter callback for scandir: take only files into account which begin with a
 * number and end with ".conf"
 * @param ent directory entry
 * @return 1 if filename begins with a number and ends with ".conf", 0 otherwise
 */
static int filterDirectory(const struct dirent * ent)
{
	if (ent->d_type != DT_REG)
		return 0;
	char first = ent->d_name[0];
	if (first < '0' || first > '9') /* does not begin with a number */
		return 0;

	size_t len = strlen(ent->d_name);
	if (len < 5)
		return 0;
	if (strcmp(ent->d_name + len - 5, ".conf")) /* does not end with ".conf" */
		return 0;

	return 1;
}

/** Compare callback for scandir: sort files by their prefix.
 * @param a directory entry (lhs)
 * @param b directory entry (rhs)
 * @return -1 if a < b, 0 if a = b, 1 if a > b where the real comparison
 *  is on the numbers preceding the filenames of a and b.
 */
static int compareDirectory(const struct dirent ** a, const struct dirent ** b)
{
	unsigned long int idx_a = strtoul((*a)->d_name, NULL, 10);
	unsigned long int idx_b = strtoul((*b)->d_name, NULL, 10);
	return idx_a < idx_b ? -1 : idx_a == idx_b ? 0 : 1;
}

/** Read all files in a directory.
 * @param path directory path
 * @param warnUnknownSection warn if section is unknown
 * @param warnUnknownOption warn if options is unknown
 * @param onErrorUseDefault if option value cannot be parsed, use default value
 *  Otherwise, parsing that file will fail
 * @param stopOnFirstError fail parsing on first error or try to resume with
 *  next file(s)
 * @return true if parsing ALL files succeeded
 */
bool CFG_readDirectory(const char * path, bool warnUnknownSection,
	bool warnUnknownOption, bool onErrorUseDefault,
	bool stopOnFirstError)
{
	/* get all filenames which begin with a number and end with ".conf"
	 * and sort them according to their number
	 */
	struct dirent ** namelist;
	int s = scandir(path, &namelist, &filterDirectory, &compareDirectory);
	if (s < 0) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not traverse path '%s'", path);
		return false;
	}

	/* prepare filename */
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

	/* iterate over all files */
	bool ret = false;
	int n;
	for (n = 0; n < s; ++n) {
		/* build filename */
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

		/* read file */
		if (!CFG_readFile(filename, warnUnknownSection, warnUnknownOption,
			onErrorUseDefault) && stopOnFirstError)
			goto ret;
	}
	ret = true;

ret:
	/* free filenames */
	for (n = 0; n < s; ++n)
		free(namelist[n]);
	free(namelist);

	return ret;
}

/** Check two strings for equality (neglecting the case) where one string is
 * not NUL-terminated.
 * @param str1 NUL-terminated string to compare
 * @param str2 non NUL-terminated string to compare with
 * @param str2len length of second string
 * @return true if strings are equal
 */
static bool isSame(const char * str1, const char * str2, size_t str2len)
{
	size_t str1len = strlen(str1);
	return str1len == str2len && !strncasecmp(str2, str1, str2len);
}

/** Print an error message from the parser.
 * @param logLevel log level
 * @param fmt format
 */
__attribute__((format(printf, 2, 3)))
static void err(enum LOG_LEVEL logLevel, const char * fmt, ...) {
	char buf[512];

	/* build message */
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	/* print message with file and line number information */
	LOG_logf(logLevel, PFX, "FILE '%s', Line %" PRIuFAST32 ": %s",
		fileName, bufferLine, buf);
}

/** Parse a section name.
 * @return section state
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

	/* advance parser */
	bufferInput += len + 3;
	bufferSize -= len + 3;
	++bufferLine;

	/* search section: check if there is at least one section with that name */
	const struct Section * s;
	LIST_foreachItem(&sections, s, list)
		if (isSame(s->section->name, buf, len))
				break;
	if (LIST_link(s, list) == LIST_end(&sections)) {
		if (optWarnUnknownSection)
			err(LOG_LEVEL_WARN, "Section %.*s is unknown", (int)len, buf);
		return SECTION_STATE_UNKNOWN;
	}

	/* set section name and name length */
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
 * @param value value to be parsed
 * @param valLen value length
 * @param ret parsed integer. Only written if parsing was successful
 * @return true if parsing was successful, false otherwise
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
 * @param value value to be parsed
 * @param valLen value length
 * @param ret parsed port. Only written if parsing was successful
 * @return true if parsing was successful, false otherwise
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
 * @param value value to be parsed
 * @param valLen value length
 * @param ret parsed boolean. Only written if parsing was successful
 * @return true if parsing was successful, false otherwise
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
 * @param value value to be parsed
 * @param valLen value length
 * @param str string to read into (will be nul-terminated), only written if
 *  parsing was successful.
 * @param sz size of the string
 * @return true if parsing was successful, false otherwise
 */
static inline bool parseString(const char * value, size_t valLen, char * str,
	size_t sz)
{
	if (valLen > sz - 1) {
		err(LOG_LEVEL_ERROR, "Value too long (max. length %" PRI_SIZE_T
			" expected)", sz - 1);
		return false;
	}
	memcpy(str, value, valLen);
	str[valLen] = '\0';
	return true;
}

/** Parse a double. Garbage after the number is ignored.
 * @param value value to be parsed
 * @param valLen value length
 * @param ret parsed double. Only written if parsing was successful
 * @return true if parsing was successful, false otherwise
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

/** Warn on unknown key.
 * @param key option key
 * @param keyLen length of option key
 */
static inline void unknownKey(const char * key, size_t keyLen)
{
	err(LOG_LEVEL_WARN, "Unknown key '%.*s'", (int)keyLen, key);
}

/** Parse a value string and assign it to an option
 * @param option option to be assigned
 * @param value value string
 * @param valLen length of value string
 * @return true if parsing succeeded, false otherwise
 */
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

/** Get an option by its name.
 * @param section section name
 * @param sectionLen section name length
 * @param key key name
 * @param keyLen key length
 * @return option or NULL if not found
 */
static const struct CFG_Option * getOption(const char * section,
	size_t sectionLen, const char * key, size_t keyLen)
{
	const struct Section * s;
	LIST_foreachItem(&sections, s, list) {
		const struct CFG_Section * sect = s->section;
		if (!isSame(sect->name, section, sectionLen))
			continue;
		/* section name matches: search option */
		size_t i;
		for (i = 0; i < sect->n_opt; ++i) {
			const struct CFG_Option * opt = &sect->options[i];
			if (isSame(opt->name, key, keyLen)) /* found -> return it */
				return opt;
		}
		/* not found: search next section */
	}

	/* not found */
	return NULL;
}

/** Parse an option line.
 * @return true if parsing succeeded
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

		/* get option by section and key name */
		const struct CFG_Option * opt = getOption(sectionName, sectionNameLen,
			key, keyLen);
		if (opt != NULL) {
			/* found */
			if (!assignOptionFromString(opt, value, valLen)) {
				/* could not parse */
				if (optOnErrorUseDefault) /* use default */
					assignOptionFromDefault(opt);
				else /* fail */
					return false;
			} else if (opt->given) {
				*opt->given = true;
			}
		} else {
			if (optWarnUnknownOption)
				unknownKey(key, keyLen);
		}
	}

	++bufferLine;

	return true;
}

/** Read the configuration file.
 * @return true if parsing succeeded
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

/** Assign option from its default value.
 * @param opt option to be assigned
 */
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

/** Load default values for all options. */
void CFG_loadDefaults()
{
	const struct Section * s;
	/* iterate over all sections and options */
	LIST_foreachItem(&sections, s, list) {
		size_t i;
		for (i = 0; i < s->section->n_opt; ++i)
			assignOptionFromDefault(&s->section->options[i]);
	}
}

/** Get option by name. Wrapper for getOption which takes NUL-terminated
 * strings and fails if the option was not found.
 * @param section section name
 * @param key key name
 * @return option
 */
static const struct CFG_Option * getOptionByString(const char * section,
	const char * key)
{
	const struct CFG_Option * opt = getOption(section, strlen(section),
		key, strlen(key));
	if (opt == NULL)
		LOG_logf(LOG_LEVEL_EMERG, PFX, "Could not find option %s.%s\n", section,
			key);
	return opt;
}

/** Set a boolean option. Option must exist and its type must be boolean.
 * @param section section name
 * @param key key name
 * @param value value to be assigned
 */
void CFG_setBoolean(const char * section, const char * key, bool value)
{
	const struct CFG_Option * opt = getOptionByString(section, key);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_BOOL);
		((union CFG_Value*)opt->var)->boolean = value;
	}
}

/** Set an integer option. Option must exist and its type must be integer.
 * @param section section name
 * @param key key name
 * @param value value to be assigned
 */
void CFG_setInteger(const char * section, const char * key, uint32_t value)
{
	const struct CFG_Option * opt = getOptionByString(section, key);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_INT);
		((union CFG_Value*)opt->var)->integer = value;
	}
}

/** Set a port option. Option must exist and its type must be port.
 * @param section section name
 * @param key key name
 * @param value value to be assigned
 */
void CFG_setPort(const char * section, const char * key, uint_fast16_t value)
{
	const struct CFG_Option * opt = getOptionByString(section, key);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_PORT);
		((union CFG_Value*)opt->var)->port = value;
	}
}

/** Set a string option. Option must exist and its type must be string.
 * @param section section name
 * @param key key name
 * @param value value to be assigned
 */
void CFG_setString(const char * section, const char * key, const char * value)
{
	const struct CFG_Option * opt = getOptionByString(section, key);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_STRING);
		strncpy(opt->var, value, opt->maxlen);
	}
}

/** Set a double option. Option must exist and its type must be double.
 * @param section section name
 * @param key key name
 * @param value value to be assigned
 */
void CFG_setDouble(const char * section, const char * key, double value)
{
	const struct CFG_Option * opt = getOptionByString(section, key);
	if (opt) {
		assert(opt->type == CFG_VALUE_TYPE_DOUBLE);
		((union CFG_Value*)opt->var)->dbl = value;
	}
}

/** Call fix-callback on each section */
static void fix()
{
	const struct Section * s;
	LIST_foreachItem(&sections, s, list) {
		const struct CFG_Section * sect = s->section;
		if (sect->fix)
			sect->fix(sect);
	}
}

/** Check the current configuration.
 * @note Should be called after CFG_readFile()
 * @return true if configuration is correct, false otherwise.
 */
bool CFG_check()
{
	const struct Section * s;
	LIST_foreachItem(&sections, s, list) {
		const struct CFG_Section * sect = s->section;
		if (sect->check && !sect->check(s->section))
			return false;
	}

	return true;
}

/** Write all options of a section with their current value into a configuration
 * file.
 * @param file file handle
 * @param section section to be written
 * @return true if writing succeeded
 */
static bool writeOptions(FILE * file, const struct CFG_Section * section)
{
	size_t n;
	for (n = 0; n < section->n_opt; ++n) {
		const struct CFG_Option * opt = &section->options[n];
		const union CFG_Value * val = opt->var;
		/* write option name */
		fprintf(file, "%s = ", opt->name);

		/* write option value, depending on its type */
		switch (opt->type) {
		case CFG_VALUE_TYPE_BOOL:
			if (fprintf(file, "%s\n", val->boolean ? "true" : "false") < 0)
				return false;
			break;
		case CFG_VALUE_TYPE_INT:
			if (fprintf(file, "%" PRIu32 "\n", val->integer) < 0)
				return false;
			break;
		case CFG_VALUE_TYPE_PORT:
			if (fprintf(file, "%" PRIuFAST16 "\n", val->port) < 0)
				return false;
			break;
		case CFG_VALUE_TYPE_STRING:
			if (fprintf(file, "%s\n", (const char*)opt->var) < 0)
				return false;
			break;
		case CFG_VALUE_TYPE_DOUBLE:
			if (fprintf(file, "%f\n", val->dbl) < 0)
				return false;
			break;
		}
	}
	return true;
}

/** Write all sections with their options into a configuration file.
 * @param file file handle
 * @return true if writing succeeded
 */
static bool writeSections(FILE * file)
{
	bool first = true;

	const struct Section * s;
	LIST_foreachItem(&sections, s, list) {
		const struct CFG_Section * sect = s->section;

		/* collect sections: write this section only if it was not already
		 * written before (see below) */
		const struct Section * prev = s;
		LIST_foreachItemRev_continue(&sections, prev, list)
			if (!strcasecmp(sect->name, prev->section->name))
				break;
		if (LIST_link(prev, list) != LIST_end(&sections))
			continue;

		/* seperator */
		if (!first)
			if (fprintf(file, "\n\n") < 0)
				return false;
		first = false;

		/* write section name */
		if (fprintf(file, "[%s]\n", sect->name) < 0)
			return false;

		/* write options */
		if (writeOptions(file, sect))
			return false;

		/* write all options of all sections with the same name */
		const struct Section * next = s;
		LIST_foreachItem_continue(&sections, next, list)
			if (!strcasecmp(sect->name, next->section->name))
				if (!writeOptions(file, next->section))
					return false;
	}
	return true;
}

/** Write all sections with their options into a configuration file.
 * @param filename configuration filename
 * @return true if writing succeeded
 */
bool CFG_write(const char * filename)
{
	FILE * file = fopen(filename, "w");
	if (!file) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not write configuration file "
			"'%s'", filename);
		return false;
	}

	bool rc = writeSections(file);

	fclose(file);

	return rc;
}

/** Write all options of a particular section into a configuration file.
 * @param filename configuration filename
 * @param section section to be written
 * @return true if writing succeeded
 */
bool CFG_writeSection(const char * filename, const struct CFG_Section * section)
{
	FILE * file = fopen(filename, "w");
	if (!file) {
		LOG_errno(LOG_LEVEL_ERROR, PFX, "Could not open configuration file "
			"'%s' for writing", filename);
		return false;
	}

	bool rc = fprintf(file, "[%s]\n", section->name) >= 0 &&
		writeOptions(file, section);

	fclose(file);

	return rc;
}
