/* Copyright (c) 2015 Sero Systems <contact at sero-systems dot de> */

#include <gps.h>
#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <gps_parser.h>

static void timeFrame(const uint8_t * buf);
static void posFrame(const uint8_t * buf);

static void construct();
static void destruct();
static void mainloop();

struct Component GPS_comp = {
	.description = "GPS",
	.construct = &construct,
	.destruct = &destruct,
	.main = &mainloop
};

static const time_t gpsEpoch = 315964800;

_Atomic enum GPS_TIME_FLAGS GPS_timeFlags = ATOMIC_VAR_INIT(GPS_TIME_FLAG_NONE);

static void construct()
{
	GPS_PARSER_init();
}

static void destruct()
{
	GPS_PARSER_destruct();
}

static void mainloop()
{
	while (true) {
		uint8_t buf[1024];
		size_t len = GPS_PARSER_getFrame(buf, sizeof buf);
		if (!len)
			continue;
		switch (buf[0]) {
		case 0x8f:
			if (len < 2)
				continue;
			switch (buf[1]) {
			case 0xab:
				if (len == 18)
					timeFrame(buf + 1);
				break;
			case 0xac:
				if (len == 69)
					posFrame(buf + 1);
				break;
			/*case 0xa2:
				if (len == 3)
					ioFrame2(buf + 1);*/
			}
			break;
		case 0x46:
			//statFrame(buf);
			break;
		case 0x55:
			//ioFrame(buf);
			break;
		}
	}
}

static void timeFrame(const uint8_t * buf)
{
	uint_fast32_t secondsOfWeek = GPS_tou32(buf + 1);
	uint_fast16_t week = GPS_tou16(buf + 5);
	int_fast16_t offset = GPS_toi16(buf + 7);
	bool testMode = !!(buf[9] & 0x10);
	bool hasOffset = !(buf[9] & 0x08);
	bool hasGpsTime = !(buf[9] & 0x04);
	bool isUTCTime = !!(buf[9] & 0x01);
	bool hasWeek = week != 1024;

	enum GPS_TIME_FLAGS flags;
	flags = (testMode ? 0 : GPS_TIME_FLAG_NON_TEST_MODE) |
		(hasGpsTime ? GPS_TIME_FLAG_HAS_GPS_TIME : 0) |
		(hasWeek ? GPS_TIME_FLAG_HAS_WEEK : 0) |
		(hasOffset ? GPS_TIME_FLAG_HAS_OFFSET : 0) |
		(isUTCTime ? GPS_TIME_FLAG_IS_UTC : 0);
	atomic_store_explicit(&GPS_timeFlags, flags, memory_order_relaxed);
#if 0
	enum GPS_TIME_STATUS stat = preGpsTime ? GPS_TIME_PRE : hasGpsTime ? GPS_TIME_FULL : GPS_TIME_NONE;

	if (!testMode) {
		//printf("isUTC: %d 0x%02x\n", isUTCTime, buf[9]);
		time_t unixtime = secondsOfWeek + week * 7 * 24 * 60 * 60 - offset +
			gps0;
		pthread_mutex_lock(&GPS_gps.mutex);
		GPS_gps.unixtime = unixtime;
		if (stat != GPS_gps.timeStatus)
			printf("GPS: timeStatus changed from %d to %d -> %s", GPS_gps.timeStatus, stat,
				ctime(&unixtime));
		GPS_gps.timeStatus = stat;
		if (hasOffset != GPS_gps.hasOffset)
			printf("GPS: hasOffset changed from %d to %d\n", GPS_gps.hasOffset, hasOffset);
		GPS_gps.hasOffset = hasOffset;
		pthread_mutex_unlock(&GPS_gps.mutex);
	}

	/*if (++GPS_gps.ctr == 10) {
		GPS_gps.ctr = 0;
		const char buf[] = { 0x10, 0x35, 0x10, 0x03 };
		(void)write(fd, buf, sizeof buf);

		const uint8_t setio3[] = { 0x10, 0x8e, 0xa2, 0x10, 0x03 };
		(void)write(fd, setio3, sizeof setio3);
	}*/
#endif
}

#define R2D 57.2957795130823208767981548141051703

static void posFrame(const uint8_t * buf)
{
	double latitude = GPS_todouble(buf + 36) * R2D;
	double longitude = GPS_todouble(buf + 44) * R2D;
	//double alt = todouble(buf + 52);
	uint16_t alarms = GPS_tou16(buf + 10);
	bool antError = !!(alarms & 3);
	bool fixes = buf[12] == 0;

#if 0
	pthread_mutex_lock(&GPS_gps.mutex);
	if (antError != GPS_gps.antennaError)
		printf("GPS: antennaError changed from %d to %d\n", GPS_gps.antennaError, antError);
	GPS_gps.antennaError = antError;
	if (fixes != GPS_gps.hasPosition)
		printf("GPS: hasPosition changed from %d to %d -> %.2f %.2f\n",
			GPS_gps.hasPosition, fixes, latitude, longitude);
	GPS_gps.hasPosition = fixes;
	GPS_gps.latitude = latitude;
	GPS_gps.longitude = longitude;
	pthread_mutex_unlock(&GPS_gps.mutex);
#endif
}


