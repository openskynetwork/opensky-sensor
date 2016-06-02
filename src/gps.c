/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <gps.h>
#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <threads.h>
#include <inttypes.h>
#include <gps_parser.h>
#include <network.h>
#include <stdio.h>

//static void timeFrame(const uint8_t * buf);
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

/** Receiver Mode */
enum GPS_RECV_MODE {
	/** Automatic (2d/3d) */
	GPS_RECV_MODE_AUTOMATIC = 0,
	/** Single Satellite (Time) */
	GPS_RECV_MODE_SNGL_SAT = 1,
	/** Horizontal (2d) */
	GPS_RECV_MODE_HORIZONTAL = 3,
	/** Full Position (3d) */
	GPS_RECV_MODE_FULL_POS = 4,
	/** Over-Determined Clock */
	GPS_RECV_MODE_ODT_CLK = 7,
};

/** Disciplining Mode */
enum GPS_DISC_MODE {
	/** Normal (Locked to GPS) */
	GPS_DISC_MODE_LOCKED = 0,
	/** Power Up */
	GPS_DISC_MODE_PWR_UP = 1,
	/** Auto Holdover */
	GPS_DISC_MODE_AUTO_HOLDOVER = 2,
	/** Manual Holdover */
	GPS_DISC_MODE_MANUAL_HOLDOVER = 3,
	/** Recovery */
	GPS_DISC_MODE_RECOVERY = 4,
	/** Disciplining disabled */
	GPS_DISC_MODE_DISC_DISABLED = 6
};

/** Critical Alarms */
enum GPS_CRIT_ALARM {
	/** Oscillator Control Voltage is at rail */
	GPS_CRIT_ALARM_DAC_AT_RAIL = 1 << 4
};

/** Minor Alarms */
enum GPS_MIN_ALARM {
	/** Oscillator Control Voltage is near a rail. Oscillator is within
	 * 2 years of becoming untunable.
	 */
	GPS_MIN_ALARM_DAC_NEAR_RAIL = 1 << 0,
	/** Antenna input is not drawing sufficient current, most likely
	 * unconnected
	 */
	GPS_MIN_ALARM_ANT_OPEN = 1 << 1,
	/** Antenna input is drawing too much current, most likely broken cable or
	 * antenna
	 */
	GPS_MIN_ALARM_ANT_SHORT = 1 << 2,
	/** No satellites are usable */
	GPS_MIN_ALARM_NOT_TRACKING_SATS = 1 << 3,
	GPS_MIN_ALARM_NOT_DISC_OSC = 1 << 4,
	/** Self-Survey procedure is in progress */
	GPS_MIN_ALARM_SURVEY_IN_PROGRESS = 1 << 5,
	/** No accurate position stored in the flash ROM */
	GPS_MIN_ALARM_NO_STORED_POSITION = 1 << 6,
	/** Leap second transition is pending */
	GPS_MIN_ALARM_LEAP_SECOND_PEND = 1 << 7,
	/** the receive is operating in one of its test modes */
	GPS_MIN_ALARM_IN_TEST_MODE = 1 << 8,
	/** Accuracy of the position used for time only fixes is questionable */
	GPS_MIN_ALARM_POSITION_QUESTIONABLE = 1 << 9,
	GPS_MIN_ALARM_ALMANAC_INCOMPLETE = 1 << 11,
	GPS_MIN_ALARM_PPS_NOT_GENERATED = 1 << 12
};

/** GPS Decoding Status */
enum GPS_DECODE_STATUS {
	/** Doing fixes */
	GPS_DECODE_STATUS_DOING_FIXES = 0x00,
	/** Don't have GPS time */
	GPS_DECODE_STATUS_NO_GPS_TIME = 0x01,
	/** PDOP too high */
	GPS_DECODE_STATUS_PDOP_TOO_HIGH = 0x03,
	/** No usable sats */
	GPS_DECODE_STATUS_NO_SATS = 0x08,
	/** Only 1 usable sat */
	GPS_DECODE_STATUS_1SAT = 0x09,
	/** Only 2 usable sats */
	GPS_DECODE_STATUS_2SAT = 0x0a,
	/** Only 3 usable sats */
	GPS_DECODE_STATUS_3SAT = 0x0b,
	/** The chosen sat is unusable */
	GPS_DECODE_STATUS_UNUSABLE = 0x0c,
	/** TRAIM rejected the fix */
	GPS_DECODE_STATUS_TRAIM_REJECT = 0x10
};

/** Disciplining Activity */
enum GPS_DISC_ACTIVITY {
	/** Phase locking */
	GPS_DISC_ACTIVITY_PHASE_LOCKING = 0,
	/** Oscillator warm-up */
	GPS_DISC_ACTIVITY_OSC_WARMUP = 1,
	/** Frequency locking */
	GPS_DISC_ACTIVITY_FREQ_LOCK = 2,
	/** Placing PPS */
	GPS_DISC_ACTIVITY_PLACING_PPS = 3,
	/** Initializing loop filter */
	GPS_DISC_ACTIVITY_INIT_LOOP_FILTER = 4,
	/** Compensating OCXO (holdover) */
	GPS_DISC_ACTIVITY_COMP_OCXO = 5,
	/** Inactive */
	GPS_DISC_ACTIVITY_INACTIVE = 6,
	/** Recovery mode */
	GPS_DISC_ACTIVITY_RECOVERY = 8,
	/** Calibration/control voltage */
	GPS_DISC_ACTIVITY_CALIBRATION = 9
};

//static const time_t gpsEpoch = 315964800;

#define R2D 57.2957795130823208767981548141051703

//_Atomic enum GPS_TIME_FLAGS GPS_timeFlags = ATOMIC_VAR_INIT(GPS_TIME_FLAG_NONE);

static pthread_mutex_t posMutex = PTHREAD_MUTEX_INITIALIZER;
static struct GPS_RawPosition position;
static bool hasPosition;

static bool needPosition;

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
	needPosition = false;

	while (true) {
		GPS_PARSER_connect();
		hasPosition = false;

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
				/*case 0xab:
					if (len == 18)
						timeFrame(buf + 1);
					break;*/
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
}

#if 0
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
#endif

static void posFrame(const uint8_t * buf)
{
	enum GPS_RECV_MODE recvMode = buf[1];
	enum GPS_DISC_MODE discMode = buf[2];
	//uint8_t selfSurveyProgress = buf[3];
	//uint32_t holdOverDuration = GPS_tou32(buf + 4);
	uint16_t critAlarms = GPS_tou16(buf + 8);
	uint16_t minorAlarms = GPS_tou16(buf + 10);
	enum GPS_DECODE_STATUS decodingStatus = buf[12];
	//enum GPS_DISC_ACTIVITY discActivity = buf[13];
	/*float ppsOffset = GPS_tofloat(buf + 16);
	float clockOffset = GPS_tofloat(buf + 20);
	uint32_t dacValue = GPS_tou32(buf + 24);
	float dacVoltage = GPS_tofloat(buf + 28);
	float temperature = GPS_tofloat(buf+ 32);*/
	double latitude = GPS_todouble(buf + 36) * R2D;
	double longitude = GPS_todouble(buf + 44) * R2D;
	double altitude = GPS_todouble(buf + 52);
	//float ppsQuantizationError = GPS_tofloat(buf + 60);

	bool selfSurvey = !!(minorAlarms & GPS_MIN_ALARM_SURVEY_IN_PROGRESS);
	bool inTestMode = !!(minorAlarms & GPS_MIN_ALARM_IN_TEST_MODE);

	/*NOC_printf("GPS Pos: %d %d ", recvMode == GPS_RECV_MODE_ODT_CLK,
		decodingStatus == GPS_DECODE_STATUS_DOING_FIXES);
	NOC_printf("LLA: %+6.2f %+6.2f %+6.2f\n", latitude, longitude,
		altitude);*/

	pthread_mutex_lock(&posMutex);
	if (critAlarms || inTestMode || recvMode != GPS_RECV_MODE_ODT_CLK ||
		discMode != GPS_DISC_MODE_LOCKED) {
		/* unrecoverable or not automatically solvable by now */
		pthread_mutex_unlock(&posMutex);
		return;
	}

	if (selfSurvey || decodingStatus != GPS_DECODE_STATUS_DOING_FIXES ||
		discMode != GPS_DISC_MODE_LOCKED) {
		/* no position -> wait */
		pthread_mutex_unlock(&posMutex);
		return;
	}

	GPS_fromdouble(latitude, (uint8_t*)&position.latitude);
	GPS_fromdouble(longitude, (uint8_t*)&position.longitute);
	GPS_fromdouble(altitude, (uint8_t*)&position.altitude);

	if (!hasPosition) {
		NOC_printf("GPS: Got LLA: %+6.2f %+6.2f %+6.2f\n", latitude, longitude,
			altitude);
	}
	hasPosition = true;
	pthread_mutex_unlock(&posMutex);

	if (hasPosition && needPosition) {
		needPosition = !NET_sendPosition(&position);
	}

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

bool GPS_getRawPosition(struct GPS_RawPosition * rawPos)
{
	pthread_mutex_lock(&posMutex);
	if (!hasPosition) {
		pthread_mutex_unlock(&posMutex);
		return false;
	}
	memcpy(rawPos, &position, sizeof *rawPos);
	pthread_mutex_unlock(&posMutex);
	return true;
}

void GPS_setNeedPosition()
{
	needPosition = true;
}
