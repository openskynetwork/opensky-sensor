/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <log.h>
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
#include <adsb.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <cfgfile.h>
#include <util.h>

static const char PFX[] = "CFG";

struct CFG_Config CFG_config;

/** Pointers into the raw configuration file for option parsing */
struct Option {
	/** Length of the Key */
	ptrdiff_t keyLen;
	/** Pointer to the Key */
	const char * key;
	/** Length of the Value */
	ptrdiff_t valLen;
	/** Pointer to the Value */
	const char * val;
};

/** Section indices */
enum SECTION {
	/** no section selected */
	SECTION_NONE = 0,
	/** Section Watchdog */
	SECTION_WD,
	/** Section FPGA */
	SECTION_FPGA,
	/** Section INPUT */
	SECTION_INPUT,
	/** Section Receiver */
	SECTION_RECV,
	/** Section Network */
	SECTION_NET,
	/** Section Buffer */
	SECTION_BUF,
	/** Section Device */
	SECTION_DEVICE,
	/** Section Statistics */
	SECTION_STAT,
	/** Section GPS */
	SECTION_GPS,

	/** Number of sections */
	SECTIONS,

	/** Section not known */
	SECTION_UNKNOWN = SECTIONS
};

/** Current begin of parser */
static const char * bufferInput;
/** Remaining size of the file to be parsed */
static off_t bufferSize;
/** Current line number */
static uint_fast32_t bufferLine;

/** Description of a section */
struct Section {
	/** Section name */
	const char * name;
	/** Section parser function */
	void (*parse)(const struct Option * option, struct CFG_Config * cfg);
};

static void loadDefaults(struct CFG_Config * cfg);
static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg);
static void fix(struct CFG_Config * cfg);
static bool check(const struct CFG_Config * cfg);

static void parseOptionWD(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionFPGA(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionINPUT(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionRECV(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionNET(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionBUF(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionDEV(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionSTAT(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionGPS(const struct Option * opt, struct CFG_Config * cfg);
static void parseOptionUnknown(const struct Option * opt,
	struct CFG_Config * cfg);

/** Description of all sections */
static const struct Section sections[] = {
	[SECTION_NONE] = { NULL, NULL },
	[SECTION_WD] = { "WATCHDOG", &parseOptionWD },
	[SECTION_FPGA] = { "FPGA", &parseOptionFPGA },
	[SECTION_INPUT] = { "INPUT", &parseOptionINPUT },
	[SECTION_RECV] = { "RECEIVER", &parseOptionRECV },
	[SECTION_NET] = { "NETWORK", &parseOptionNET },
	[SECTION_BUF] = { "BUFFER", &parseOptionBUF },
	[SECTION_DEVICE] = { "DEVICE", &parseOptionDEV },
	[SECTION_STAT] = { "STATISTICS", &parseOptionSTAT },
	[SECTION_GPS] = { "GPS", &parseOptionGPS },
	[SECTION_UNKNOWN] = { NULL, &parseOptionUnknown }
};

/** Load default configuration values.
 * \note this should be called prior to CFG_readFile()
 */
void CFG_loadDefaults()
{
	loadDefaults(&CFG_config);
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
	readCfg(cfgStr, st.st_size, &CFG_config);

	/* unmap file */
	munmap(cfgStr, st.st_size);

	/* fix configuration */
	fix(&CFG_config);

	return true;
}

/** Check the current configuration.
 * \note Should be called after CFG_readFile()
 * \return true if configuration is correct, false otherwise.
 */
bool CFG_check()
{
	/* check configuration */
	return check(&CFG_config);
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
static const struct Section * parseSection()
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
	enum SECTION sect;
	for (sect = 1; sect < SECTIONS; ++sect)
		if (isSame(sections[sect].name, buf, len))
			break;

	if (sect == SECTION_UNKNOWN)
		LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": Section %.*s is "
			"unknown", bufferLine, (int)len, buf);

	/* advance parser and return */
	bufferInput += len + 3;
	bufferSize -= len + 3;
	++bufferLine;

	return &sections[sect];
}

/** Scan a comment. This will just advance the parser to the end of line/file */
static void scanComment()
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
static inline bool parseInt(const struct Option * opt, uint32_t * ret)
{
	char buf[20];
	if (opt->valLen + 1 > sizeof buf || opt->valLen == 0) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": Number expected",
			bufferLine);
		return false;
	}
	strncpy(buf, opt->val, opt->valLen);
	buf[opt->valLen] = '\0';

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
static inline bool parsePort(const struct Option * opt, uint16_t * ret)
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

/** Parse a boolean.
 * \param opt option to be parsed
 * \param ret parsed boolean. Only written if parsing was successful
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseBool(const struct Option * opt, bool * ret)
{
	if (isSame("true", opt->val, opt->valLen)
		|| isSame("1", opt->val, opt->valLen)) {
		*ret = true;
		return true;
	}
	if (isSame("false", opt->val, opt->valLen)
		|| isSame("0", opt->val, opt->valLen)) {
		*ret = false;
		return true;
	}

	LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": boolean option has "
		"unexpected value '%.*s'", bufferLine, (int)opt->valLen, opt->val);
	return false;
}

/** Parse a string.
 * \param opt option to be parsed
 * \param str string to read into (will be nul-terminated), only written if
 *  parsing was successful.
 * \param sz size of the string
 * \return true if parsing was successful, false otherwise
 */
static inline bool parseString(const struct Option * opt, char * str, size_t sz)
{
	if (opt->valLen > sz - 1) {
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Line %" PRIuFAST32 ": Value too long "
			"(max. length %zu expected)", bufferLine, sz - 1);
		return false;
	}
	memcpy(str, opt->val, opt->valLen);
	str[opt->valLen] = '\0';
	return true;
}

/** Test for an option's key.
 * \param option option to be tested
 * \param name key name to be compared to
 * \return true if option matches the keyname
 */
static inline bool isOption(const struct Option * opt, const char * name)
{
	return isSame(name, opt->key, opt->keyLen);
}

/** Stop on unknown key.
 * \param opt option which couldn't be recognized
 */
static inline void unknownKey(const struct Option * opt)
{
	LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": unknown key '%.*s'",
		bufferLine, (int)opt->keyLen, opt->key);
}

/** Parse Watchdog Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionWD(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "enabled"))
		parseBool(opt, &cfg->wd.enabled);
	else
		unknownKey(opt);
}

/** Scan FPGA Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionFPGA(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "configure"))
		parseBool(opt, &cfg->fpga.configure);
	else if (isOption(opt, "file"))
		parseString(opt, cfg->fpga.file, sizeof cfg->fpga.file);
	else if (isOption(opt, "retries"))
		parseInt(opt, &cfg->fpga.retries);
	else if (isOption(opt, "timeout"))
		parseInt(opt, &cfg->fpga.timeout);
	else
		unknownKey(opt);
}

/** Scan INPUT Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionINPUT(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "uart")) {
#ifndef INPUT_LAYER_NETWORK
		parseString(opt, cfg->input.uart, sizeof cfg->input.uart);
#endif
	} else if (isOption(opt, "host")) {
#ifdef INPUT_LAYER_NETWORK
		parseString(opt, cfg->input.host, sizeof cfg->input.host);
#endif
	} else if (isOption(opt, "port")) {
#ifdef INPUT_LAYER_NETWORK
		parsePort(opt, &cfg->input.port);
#endif
	} else if (isOption(opt, "rtscts")) {
#ifndef INPUT_LAYER_NETWORK
		parseBool(opt, &cfg->input.rtscts);
#endif
	} else if (isOption(opt, "reconnectInterval"))
		parseInt(opt, &cfg->input.reconnectInterval);
	else
		unknownKey(opt);
}

/** Scan Receiver Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionRECV(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "ModeS_Long_ExtSquitterOnly"))
		parseBool(opt, &cfg->recv.modeSLongExtSquitter);
	else if (isOption(opt, "SynchronizationFilter"))
		parseBool(opt, &cfg->recv.syncFilter);
	else if (isOption(opt, "crc"))
		parseBool(opt, &cfg->recv.crc);
	else if (isOption(opt, "fec"))
		parseBool(opt, &cfg->recv.fec);
	else if (isOption(opt, "gps"))
		parseBool(opt, &cfg->recv.gps);
	else
		unknownKey(opt);
}

/** Scan Network Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionNET(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "host"))
		parseString(opt, cfg->net.host, sizeof cfg->net.host);
	else if (isOption(opt, "port")) {
		parsePort(opt, &cfg->net.port);
	} else if (isOption(opt, "timeout"))
		parseInt(opt, &cfg->net.timeout);
	else if (isOption(opt, "reconnectInterval"))
		parseInt(opt, &cfg->net.reconnectInterval);
	else
		unknownKey(opt);
}

/** Scan Buffer Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionBUF(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "history"))
		parseBool(opt, &cfg->buf.history);
	else if (isOption(opt, "staticBacklog"))
		parseInt(opt, &cfg->buf.statBacklog);
	else if (isOption(opt, "dynamicBacklog"))
		parseInt(opt, &cfg->buf.dynBacklog);
	else if (isOption(opt, "dynamicIncrement"))
		parseInt(opt, &cfg->buf.dynIncrement);
	else if (isOption(opt, "gcEnabled"))
		parseBool(opt, &cfg->buf.gcEnabled);
	else if (isOption(opt, "gcInterval"))
		parseInt(opt, &cfg->buf.gcInterval);
	else if (isOption(opt, "gcLevel"))
		parseInt(opt, &cfg->buf.gcLevel);
	else
		unknownKey(opt);
}

/** Scan Device Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionDEV(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "serial")) {
		cfg->dev.serialSet = parseInt(opt, &cfg->dev.serial);
	} else if (isOption(opt, "device")) {
		parseString(opt, cfg->dev.deviceName, sizeof cfg->dev.deviceName);
	} else
		unknownKey(opt);
}

/** Scan Statistics Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionSTAT(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "enabled"))
		parseBool(opt, &cfg->stats.enabled);
	else if (isOption(opt, "interval"))
		parseInt(opt, &cfg->stats.interval);
	else
		unknownKey(opt);
}

/** Scan GPS Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionGPS(const struct Option * opt, struct CFG_Config * cfg)
{
	/* TODO: use gps specific compilation flags? */
	if (isOption(opt, "uart")) {
#ifndef INPUT_LAYER_NETWORK
		parseString(opt, cfg->gps.uart, sizeof cfg->gps.uart);
#endif
	} else if (isOption(opt, "host")) {
#ifdef INPUT_LAYER_NETWORK
		parseString(opt, cfg->gps.host, sizeof cfg->gps.host);
#endif
	} else if (isOption(opt, "port")) {
#ifdef INPUT_LAYER_NETWORK
		parsePort(opt, &cfg->gps.port);
#endif
	} else if (isOption(opt, "reconnectInterval"))
		parseInt(opt, &cfg->gps.reconnectInterval);
	else
		unknownKey(opt);
}

/** Scan Unknown Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void parseOptionUnknown(const struct Option * opt,
	struct CFG_Config * cfg)
{
}

/** Parse an option line.
 * \param section current section
 * \param cfg configuration to be filled
 */
static bool parseOption(const struct Section * section, struct CFG_Config * cfg)
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

	struct Option opt;

	/* eliminate whitespaces at the end of the key */
	const char * l = e - 1;
	while (l > bufferInput && isspace(*l))
		--l;
	opt.key = bufferInput;
	opt.keyLen = l + 1 - opt.key;

	/* eliminate whitespaces at the beginning of the value */
	const char * r = e + 1;
	while (r < n && isspace(*r))
		++r;
	opt.val = r;
	opt.valLen = n - r;

	/* parse option value */
	section->parse(&opt, cfg);

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
static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg)
{
	const struct Section * const sectionNone = &sections[SECTION_NONE];
	const struct Section * const sectionUnknown = &sections[SECTION_UNKNOWN];
	const struct Section * section = sectionNone;

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
			scanComment();
		break;
		default:
			if (section == sectionNone) {
				LOG_logf(LOG_LEVEL_WARN, PFX, "Line %" PRIuFAST32 ": "
					"Unexpected option outside any section", bufferLine);
				section = sectionUnknown;
			}
			parseOption(section, cfg);
		}
	}
}

/** Load default values.
 * \param cfg configuration
 */
static void loadDefaults(struct CFG_Config * cfg)
{
#ifdef STANDALONE
	cfg->wd.enabled = true;

	cfg->fpga.configure = true;
#else
	cfg->wd.enabled = false;

	cfg->fpga.configure = false;
#endif
	strncpy(cfg->fpga.file, "cape.rbf", sizeof cfg->fpga.file);
	cfg->fpga.retries = 2;
	cfg->fpga.timeout = 10;

#ifdef INPUT_LAYER_NETWORK
	strncpy(cfg->input.host, "localhost", sizeof cfg->input.host);
	cfg->input.port = 10003;
#else
	strncpy(cfg->input.uart, "/dev/ttyO5", sizeof cfg->input.uart);
	cfg->input.rtscts = true;
#endif
	cfg->input.reconnectInterval = 10;

	cfg->recv.modeSLongExtSquitter = true;
	cfg->recv.syncFilter = true;
	cfg->recv.crc = true;
	cfg->recv.fec = true;
	cfg->recv.gps = true;

	cfg->net.host[0] = '\0';
	cfg->net.port = 30003;
	cfg->net.timeout = 30;
	cfg->net.reconnectInterval = 10;

	cfg->buf.history = true;
	cfg->buf.statBacklog = 10;
	cfg->buf.dynBacklog = 1000;
	cfg->buf.dynIncrement = 1000;
	cfg->buf.gcEnabled = true;
	cfg->buf.gcInterval = 120;
	cfg->buf.gcLevel = 2;

	cfg->dev.serialSet = false;
	strncpy(cfg->dev.deviceName, "eth0", sizeof cfg->dev.deviceName);

	cfg->stats.enabled = true;
	cfg->stats.interval = 600;

#ifdef INPUT_LAYER_NETWORK
	strncpy(cfg->gps.host, "localhost", sizeof cfg->gps.host);
	cfg->gps.port = 10685;
#else
	strncpy(cfg->gps.uart, "/dev/ttyO2", sizeof cfg->gps.uart);
#endif
	cfg->gps.reconnectInterval = 10;
}

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

#ifdef INPUT_LAYER_NETWORK
	if (cfg->input.host[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.host is missing");
		return false;
	}
	if (cfg->input.port == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "INPUT.port = 0");
		return false;
	}
#else
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

#ifdef INPUT_LAYER_NETWORK
	if (cfg->gps.host[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.host is missing");
		return false;
	}
	if (cfg->gps.port == 0) {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.port = 0");
		return false;
	}
#else
	if (cfg->gps.uart[0] == '\0') {
		LOG_log(LOG_LEVEL_ERROR, PFX, "GPS.uart is missing");
		return false;
	}
#endif

	return true;
}

