#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <message.h>

const char * MSG_TYPE_NAMES[MSG_TYPES] = {
	[MSG_TYPE_ADSB] = "ADSB frame",
	[MSG_TYPE_LOG] = "log message",
	[MSG_TYPE_STAT] = "statistic message"
};
