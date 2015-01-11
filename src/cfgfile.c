
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <adsb.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <cfgfile.h>

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
	/** Section ADSB */
	SECTION_ADSB,
	/** Section Network */
	SECTION_NET,
	/** Section Buffer */
	SECTION_BUF,
	/** Section Device */
	SECTION_DEVICE,
	/** Section Statistics */
	SECTION_STAT,

	/** Number of sections */
	SECTIONS
};

/** Current begin of parser */
static const char * bufferInput;
/** Remaining size of the file to be parsed */
static off_t bufferSize;
/** Current line number */
static uint32_t bufferLine;

/** Description of a section */
struct Section {
	/** Section name */
	const char * name;
	/** Section parser function */
	void (*parse)(const struct Option * option, struct CFG_Config * cfg);
};

static void loadDefaults(struct CFG_Config * cfg);
static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg);
static void check(struct CFG_Config * cfg);

static void scanOptionWD(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionFPGA(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionADSB(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionNET(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionBUF(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionDEV(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionSTAT(const struct Option * opt, struct CFG_Config * cfg);

/** Description of all sections */
static const struct Section sections[] = {
	[SECTION_NONE] = { NULL, NULL },
	[SECTION_WD] = { "WATCHDOG", &scanOptionWD },
	[SECTION_FPGA] = { "FPGA", &scanOptionFPGA },
	[SECTION_ADSB] = { "ADSB", &scanOptionADSB },
	[SECTION_NET] = { "NETWORK", &scanOptionNET },
	[SECTION_BUF] = { "BUFFER", &scanOptionBUF },
	[SECTION_DEVICE] = { "DEVICE", &scanOptionDEV },
	[SECTION_STAT] = { "STATISTICS", &scanOptionSTAT },
};

/** Read and check a configuration file.
 * \param file configuration file name to be read
 * \param cfg configuration structure to be filled
 */
void CFG_read(const char * file, struct CFG_Config * cfg)
{
	/* load default values */
	loadDefaults(cfg);

	/* open input file */
	int fd = open(file, O_RDONLY);
	if (fd < 0)
		error(-1, errno, "Configuration error: could not open '%s'", file);

	/* stat input file for its size */
	struct stat st;
	if (fstat(fd, &st) < 0) {
		close(fd);
		error(-1, errno, "Configuration error: could not stat '%s'", file);
	}

	/* mmap input file */
	char * cfgStr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (cfgStr == MAP_FAILED) {
		close(fd);
		error(-1, errno, "Configuration error: could not mmap '%s'", file);
	}

	/* actually read configuration */
	readCfg(cfgStr, st.st_size, cfg);

	/* unmap and close file */
	munmap(cfgStr, st.st_size);
	close(fd);

	/* check configuration */
	check(cfg);
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

/** Scan a section name.
 * \return section index
 */
static enum SECTION scanSection()
{
	/* get next ']' terminator */
	const char * c = memchr(bufferInput, ']', bufferSize);
	/* get next newline or end-of-file */
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;

	/* sanity checks */
	if (!c || n < c)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": ] expected, but newline found", bufferLine);
	if (c != n - 1)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": newline after ] expected", bufferLine);

	/* calculate length */
	const ptrdiff_t len = c - bufferInput - 1;
	const char * const buf = bufferInput + 1;

	/* search section */
	enum SECTION sect;
	for (sect = 1; sect < SECTIONS; ++sect) {
		if (isSame(sections[sect].name, buf, len)) {
			/* found: advance parser and return */
			bufferInput += len + 3;
			bufferSize -= len + 3;
			++bufferLine;

			return sect;
		}
	}

	/* section not found */
	error(-1, 0, "Configuration error: Line %" PRIu32
		": Section %.*s is unknown", bufferLine, (int)len, buf);

	/* keep gcc happy */
	return SECTION_NONE;
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

/** Parse an integer.
 * \param opt option to be parsed
 * \return parsed integer
 */
static inline uint32_t parseInt(const struct Option * opt)
{
	char buf[20];
	if (opt->valLen + 1 > sizeof buf || opt->valLen == 0)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": Not a number", bufferLine);
	strncpy(buf, opt->val, opt->valLen);
	buf[opt->valLen] = '\0';

	char * end;
	unsigned long int n = strtoul(buf, &end, 0);
	if (*end != '\0')
		error(-1, 0, "Configuration error: Line %" PRIu32
			": Garbage trailing line", bufferLine);

	return n;
}

/** Parse a boolean.
 * \param opt option to be parsed
 * \return parsed boolean
 */
static inline bool parseBool(const struct Option * opt)
{
	if (isSame("true", opt->val, opt->valLen) ||
		isSame("1", opt->val, opt->valLen))
		return true;
	if (isSame("false", opt->val, opt->valLen) ||
		isSame("0", opt->val, opt->valLen))
		return false;

	error(-1, 0, "Configuration error: Line %" PRIu32
		": boolean option has unexpected value '%.*s'",
		bufferLine, (int)opt->valLen, opt->val);
	return false;
}

/** Parse a string.
 * \param opt option to be parsed
 * \param str string to read into (will be nul-terminated)
 * \param sz size of the string
 */
static inline void parseString(const struct Option * opt, char * str, size_t sz)
{
	if (opt->valLen > sz - 1)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": Value to long (max %zu expected)", bufferLine, sz - 1);
	memcpy(str, opt->val, opt->valLen);
	str[opt->valLen] = '\0';
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
	error(-1, 0, "Configuration error: Line %" PRIu32
		": unknown key '%.*s'", bufferLine, (int)opt->keyLen, opt->key);
}

/** Scan Watchdog Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionWD(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "enabled"))
		cfg->wd.enabled = parseBool(opt);
	else
		unknownKey(opt);
}

/** Scan FPGA Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionFPGA(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "configure"))
		cfg->fpga.configure = parseBool(opt);
	else if (isOption(opt, "file"))
		parseString(opt, cfg->fpga.file, sizeof cfg->fpga.file);
	else if (isOption(opt, "retries"))
		cfg->fpga.retries = parseInt(opt);
	else if (isOption(opt, "timeout"))
		cfg->fpga.timeout = parseInt(opt);
	else
		unknownKey(opt);
}

/** Scan ADSB Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionADSB(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "uart"))
		parseString(opt, cfg->adsb.uart, sizeof cfg->adsb.uart);
	else if (isOption(opt, "CRC"))
		cfg->adsb.crc = parseBool(opt);
	else if (isOption(opt, "FEC"))
		cfg->adsb.fec = parseBool(opt);
	else if (isOption(opt, "FrameFilter"))
		cfg->adsb.frameFilter = parseBool(opt);
	else if (isOption(opt, "ModeAC"))
		cfg->adsb.modeAC = parseBool(opt);
	else if (isOption(opt, "RTS"))
		cfg->adsb.rts = parseBool(opt);
	else if (isOption(opt, "GPS"))
		cfg->adsb.timestampGPS = parseBool(opt);
	else if (isOption(opt, "ModeS_Short"))
		cfg->adsb.modeSShort = true;
	else if (isOption(opt, "ModeS_Long"))
		cfg->adsb.modeSLong = true;
	else
		unknownKey(opt);
}

/** Scan Network Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionNET(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "host"))
		parseString(opt, cfg->net.host, sizeof cfg->net.host);
	else if (isOption(opt, "port")) {
		uint32_t n = parseInt(opt);
		if (n > 0xffff)
			error(-1, 0, "Configuration error: Line %" PRIu32
				": port must be < 65535", bufferLine);
		cfg->net.port = n;
	} else if (isOption(opt, "timeout"))
		cfg->net.timeout = parseInt(opt);
	else if (isOption(opt, "reconnectInterval"))
		cfg->net.reconnectInterval = parseInt(opt);
	else
		unknownKey(opt);
}

/** Scan Buffer Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionBUF(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "history"))
		cfg->buf.history = parseBool(opt);
	else if (isOption(opt, "staticBacklog"))
		cfg->buf.statBacklog = parseInt(opt);
	else if (isOption(opt, "dynamicBacklog"))
		cfg->buf.dynBacklog = parseInt(opt);
	else if (isOption(opt, "dynamicIncrement"))
		cfg->buf.dynIncrement = parseInt(opt);
	else if (isOption(opt, "gcEnabled"))
		cfg->buf.gcEnabled = parseBool(opt);
	else if (isOption(opt, "gcInterval"))
		cfg->buf.gcInterval = parseInt(opt);
	else if (isOption(opt, "gcLevel"))
		cfg->buf.gcLevel = parseInt(opt);
	else unknownKey(opt);
}

/** Scan Device Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionDEV(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "serial")) {
		cfg->dev.serial = parseInt(opt);
		cfg->dev.serialSet = true;
	} else
		unknownKey(opt);
}

/** Scan Statistics Section.
 * \param opt option to be parsed
 * \param cfg configuration to be filled
 */
static void scanOptionSTAT(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "enabled"))
		cfg->stats.enabled = parseBool(opt);
	else if (isOption(opt, "interval"))
		cfg->stats.interval = parseInt(opt);
	else
		unknownKey(opt);
}

/** Scan an option line.
 * \param sect index to current section
 * \param cfg configuration to be filled
 */
static void scanOption(enum SECTION sect, struct CFG_Config * cfg)
{
	/* split option on '='-sign */
	const char * e = memchr(bufferInput, '=', bufferSize);
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	/* sanity check */
	if (n < e)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": = expected, but newline found", bufferLine);

	struct Option opt;

	/* eliminate whitespaces at the end of the key */
	const char * l = e - 1;
	while (l > bufferInput && isspace(*l)) --l;
	opt.key = bufferInput;
	opt.keyLen = l + 1 - opt.key;

	/* eliminate whitespaces at the beginning of the value */
	const char * r = e + 1;
	while (r < n && isspace(*r)) ++r;
	opt.val = r;
	opt.valLen = n - r;

	/* parse option value */
	sections[sect].parse(&opt, cfg);

	/* advance buffer */
	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;
	++bufferLine;
}

/** Read the configuration file.
 * \param cfgStr configuration, loaded into memory
 * \param size size of the configuration
 * \param cfg configuration structure to be filled
 */
static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg)
{
	enum SECTION sect = SECTION_NONE;

	/* initialize parser buffer */
	bufferInput = cfgStr;
	bufferSize = size;
	bufferLine = 1;

	while (bufferSize) {
		switch (bufferInput[0]) {
		case '[':
			sect = scanSection();
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
			if (sect == SECTION_NONE)
				error(-1, 0, "Configuration error: Line %" PRIu32
					": Unexpected '%c' outside any section", bufferLine,
					bufferInput[0]);
			scanOption(sect, cfg);
		}
	}
}

/** Load default values.
 * \param cfg configuration
 */
static void loadDefaults(struct CFG_Config * cfg)
{
	cfg->wd.enabled = true;

	cfg->fpga.configure = true;
	strncpy(cfg->fpga.file, "cape.rbf", sizeof cfg->fpga.file);
	cfg->fpga.retries = 2;
	cfg->fpga.timeout = 10;

	strncpy(cfg->adsb.uart, "/dev/ttyO5", sizeof cfg->adsb.uart);
	cfg->adsb.frameFilter = true;
	cfg->adsb.crc = true;
	cfg->adsb.timestampGPS = true;
	cfg->adsb.rts = true;
	cfg->adsb.fec = true;
	cfg->adsb.modeAC = false;
	cfg->adsb.modeSShort = false;
	cfg->adsb.modeSLong = true;

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

	cfg->stats.enabled = true;
	cfg->stats.interval = 600;
}

/** Check configuration for sanity.
 * \param cfg configuration
 */
static void check(struct CFG_Config * cfg)
{
	if (cfg->fpga.configure && cfg->fpga.file[0] == '\0')
			error(-1, 0, "Configuration error: FPGA.file is empty");

	if (cfg->net.host[0] == '\0')
		error(-1, 0, "Configuration error: NET.host is empty");
	if (cfg->net.port == 0)
		error(-1, 0, "Configuration error: NET.port = 0");

	if (!cfg->dev.serialSet)
		error(-1, 0, "Configuration error: DEVICE.serial is missing");

	if (cfg->stats.interval == 0)
		error(-1, 0, "Configuration error: STATISTICS.interval is 0");
}


