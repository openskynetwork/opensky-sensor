
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

struct Option {
	ptrdiff_t keyLen;
	const char * key;
	ptrdiff_t valLen;
	const char * val;
};

enum SECTION {
	SECTION_NONE = 0,
	SECTION_WD,
	SECTION_FPGA,
	SECTION_ADSB,
	SECTION_NET,
	SECTION_BUF,
	SECTION_DEVICE,

	SECTIONS
};

static const char * bufferInput;
static off_t bufferSize;
static uint32_t bufferLine;

static void scanOptionWD(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionFPGA(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionADSB(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionNET(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionBUF(const struct Option * opt, struct CFG_Config * cfg);
static void scanOptionDEV(const struct Option * opt, struct CFG_Config * cfg);

struct Section {
	const char * name;
	void (*parse)(const struct Option * option, struct CFG_Config * cfg);
};

static const struct Section sections[] = {
	[SECTION_NONE] = { NULL, NULL },
	[SECTION_WD] = { "WATCHDOG", &scanOptionWD },
	[SECTION_FPGA] = { "FPGA", &scanOptionFPGA },
	[SECTION_ADSB] = { "ADSB", &scanOptionADSB },
	[SECTION_NET] = { "NETWORK", &scanOptionNET },
	[SECTION_BUF] = { "BUFFER", &scanOptionBUF },
	[SECTION_DEVICE] = { "DEVICE", &scanOptionDEV }
};

static void loadDefaults(struct CFG_Config * cfg);
static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg);
static void check(struct CFG_Config * cfg);

void CFG_read(const char * file, struct CFG_Config * cfg)
{
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

	readCfg(cfgStr, st.st_size, cfg);

	munmap(cfgStr, st.st_size);
	close(fd);

	check(cfg);
}

static bool isSame(const char * str1, const char * str2, size_t str2len)
{
	size_t str1len = strlen(str1);
	return str1len == str2len && !strncasecmp(str2, str1, str2len);
}

static enum SECTION scanSection()
{
	const char * c = memchr(bufferInput, ']', bufferSize);
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	if (!c || n < c)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": ] expected, but newline found", bufferLine);
	if (c != n - 1)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": newline after ] expected", bufferLine);

	const ptrdiff_t len = c - bufferInput - 1;
	const char * const buf = bufferInput + 1;

	enum SECTION ret;
	for (ret = 1; ret < SECTIONS; ++ret) {
		if (isSame(sections[ret].name, buf, len)) {
			bufferInput += len + 3;
			bufferSize -= len + 3;
			++bufferLine;

			return ret;
		}
	}

	error(-1, 0, "Configuration error: Line %" PRIu32
		": Section %.*s is unknown", bufferLine, (int)len, buf);

	return SECTION_NONE;
}

static void scanComment()
{
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;
	++bufferLine;
}

static inline uint32_t parseInt(const struct Option * opt)
{
	char buf[20];
	if (opt->valLen + 1 > sizeof buf)
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

static inline void parseString(const struct Option * opt, char * str, size_t sz)
{
	if (opt->valLen > sz - 1)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": Value to long (max %zu expected)", bufferLine, sz - 1);
	memcpy(str, opt->val, opt->valLen);
	str[opt->valLen] = '\0';
}

static inline bool isOption(const struct Option * opt, const char * name)
{
	return isSame(name, opt->key, opt->keyLen);
}

static inline void unknownKey(const struct Option * opt)
{
	error(-1, 0, "Configuration error: Line %" PRIu32
		": unknown key '%.*s'", bufferLine, (int)opt->keyLen, opt->key);
}

static void scanOptionWD(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "enabled"))
		cfg->wd.enabled = parseBool(opt);
	else
		unknownKey(opt);
}

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
	else
		unknownKey(opt);
}

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

static void scanOptionDEV(const struct Option * opt, struct CFG_Config * cfg)
{
	if (isOption(opt, "serial")) {
		cfg->dev.serial = parseInt(opt);
		cfg->dev.serialSet = true;
	} else
		unknownKey(opt);
}

static void scanOption(enum SECTION sect, struct CFG_Config * cfg)
{
	const char * e = memchr(bufferInput, '=', bufferSize);
	const char * n = memchr(bufferInput, '\n', bufferSize);
	if (n == NULL)
		n = bufferInput + bufferSize - 1;
	if (n < e)
		error(-1, 0, "Configuration error: Line %" PRIu32
			": = expected, but newline found", bufferLine);

	struct Option opt;

	const char * l = e - 1;
	while (l > bufferInput && isspace(*l)) --l;
	opt.key = bufferInput;
	opt.keyLen = l + 1 - opt.key;

	const char * r = e + 1;
	while (r < n && isspace(*r)) ++r;
	opt.val = r;
	opt.valLen = n - r;

	sections[sect].parse(&opt, cfg);

	bufferSize -= n + 1 - bufferInput;
	bufferInput = n + 1;
	++bufferLine;
}

static void readCfg(const char * cfgStr, off_t size, struct CFG_Config * cfg)
{
	enum SECTION sect = SECTION_NONE;

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
	cfg->adsb.modeAC = true;

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
}

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
}


