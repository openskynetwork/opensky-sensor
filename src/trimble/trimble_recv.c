/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#include <error.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdbool.h>
#include "core/gps.h"
#include "core/network.h"
#include "trimble_input.h"
#include "trimble_parser.h"
#include "trimble_recv.h"
#include "util/endec.h"
#include "util/threads.h"

//static void timeFrame(const uint8_t * buf);
static void posFrame(const uint8_t * buf);

static bool construct();
static void mainloop();

struct Component TRIMBLE_comp = {
	.description = "GPS",
	.onRegister = &TRIMBLE_INPUT_register,
	.onConstruct = &construct,
	.main = &mainloop,
	.dependencies = { &NET_comp, NULL }
};

/** Receiver Mode */
enum TRIMBLE_GPS_RECV_MODE {
	/** Automatic (2d/3d) */
	TRIMBLE_GPS_RECV_MODE_AUTOMATIC = 0,
	/** Single Satellite (Time) */
	TRIMBLE_GPS_RECV_MODE_SNGL_SAT = 1,
	/** Horizontal (2d) */
	TRIMBLE_GPS_RECV_MODE_HORIZONTAL = 3,
	/** Full Position (3d) */
	TRIMBLE_GPS_RECV_MODE_FULL_POS = 4,
	/** Over-Determined Clock */
	TRIMBLE_GPS_RECV_MODE_ODT_CLK = 7,
};

/** Disciplining Mode */
enum TRIMBLE_GPS_DISC_MODE {
	/** Normal (Locked to GPS) */
	TRIMBLE_GPS_DISC_MODE_LOCKED = 0,
	/** Power Up */
	TRIMBLE_GPS_DISC_MODE_PWR_UP = 1,
	/** Auto Holdover */
	TRIMBLE_GPS_DISC_MODE_AUTO_HOLDOVER = 2,
	/** Manual Holdover */
	TRIMBLE_GPS_DISC_MODE_MANUAL_HOLDOVER = 3,
	/** Recovery */
	TRIMBLE_GPS_DISC_MODE_RECOVERY = 4,
	/** Disciplining disabled */
	TRIMBLE_GPS_DISC_MODE_DISC_DISABLED = 6
};

/** Critical Alarms */
enum TRIMBLE_GPS_CRIT_ALARM {
	/** Oscillator Control Voltage is at rail */
	TRIMBLE_GPS_CRIT_ALARM_DAC_AT_RAIL = 1 << 4
};

/** Minor Alarms */
enum TRIMBLE_GPS_MIN_ALARM {
	/** Oscillator Control Voltage is near a rail. Oscillator is within
	 * 2 years of becoming untunable.
	 */
	TRIMBLE_GPS_MIN_ALARM_DAC_NEAR_RAIL = 1 << 0,
	/** Antenna input is not drawing sufficient current, most likely
	 * unconnected
	 */
	TRIMBLE_GPS_MIN_ALARM_ANT_OPEN = 1 << 1,
	/** Antenna input is drawing too much current, most likely broken cable or
	 * antenna
	 */
	TRIMBLE_GPS_MIN_ALARM_ANT_SHORT = 1 << 2,
	/** No satellites are usable */
	TRIMBLE_GPS_MIN_ALARM_NOT_TRACKING_SATS = 1 << 3,
	TRIMBLE_GPS_MIN_ALARM_NOT_DISC_OSC = 1 << 4,
	/** Self-Survey procedure is in progress */
	TRIMBLE_GPS_MIN_ALARM_SURVEY_IN_PROGRESS = 1 << 5,
	/** No accurate position stored in the flash ROM */
	TRIMBLE_GPS_MIN_ALARM_NO_STORED_POSITION = 1 << 6,
	/** Leap second transition is pending */
	TRIMBLE_GPS_MIN_ALARM_LEAP_SECOND_PEND = 1 << 7,
	/** the receive is operating in one of its test modes */
	TRIMBLE_GPS_MIN_ALARM_IN_TEST_MODE = 1 << 8,
	/** Accuracy of the position used for time only fixes is questionable */
	TRIMBLE_GPS_MIN_ALARM_POSITION_QUESTIONABLE = 1 << 9,
	TRIMBLE_GPS_MIN_ALARM_ALMANAC_INCOMPLETE = 1 << 11,
	TRIMBLE_GPS_MIN_ALARM_PPS_NOT_GENERATED = 1 << 12
};

/** GPS Decoding Status */
enum TRIMBLE_GPS_DECODE_STATUS {
	/** Doing fixes */
	TRIMBLE_GPS_DECODE_STATUS_DOING_FIXES = 0x00,
	/** Don't have GPS time */
	TRIMBLE_GPS_DECODE_STATUS_NO_TRIMBLE_GPS_TIME = 0x01,
	/** PDOP too high */
	TRIMBLE_GPS_DECODE_STATUS_PDOP_TOO_HIGH = 0x03,
	/** No usable sats */
	TRIMBLE_GPS_DECODE_STATUS_NO_SATS = 0x08,
	/** Only 1 usable sat */
	TRIMBLE_GPS_DECODE_STATUS_1SAT = 0x09,
	/** Only 2 usable sats */
	TRIMBLE_GPS_DECODE_STATUS_2SAT = 0x0a,
	/** Only 3 usable sats */
	TRIMBLE_GPS_DECODE_STATUS_3SAT = 0x0b,
	/** The chosen sat is unusable */
	TRIMBLE_GPS_DECODE_STATUS_UNUSABLE = 0x0c,
	/** TRAIM rejected the fix */
	TRIMBLE_GPS_DECODE_STATUS_TRAIM_REJECT = 0x10
};

/** Disciplining Activity */
enum TRIMBLE_GPS_DISC_ACTIVITY {
	/** Phase locking */
	TRIMBLE_GPS_DISC_ACTIVITY_PHASE_LOCKING = 0,
	/** Oscillator warm-up */
	TRIMBLE_GPS_DISC_ACTIVITY_OSC_WARMUP = 1,
	/** Frequency locking */
	TRIMBLE_GPS_DISC_ACTIVITY_FREQ_LOCK = 2,
	/** Placing PPS */
	TRIMBLE_GPS_DISC_ACTIVITY_PLACING_PPS = 3,
	/** Initializing loop filter */
	TRIMBLE_GPS_DISC_ACTIVITY_INIT_LOOP_FILTER = 4,
	/** Compensating OCXO (holdover) */
	TRIMBLE_GPS_DISC_ACTIVITY_COMP_OCXO = 5,
	/** Inactive */
	TRIMBLE_GPS_DISC_ACTIVITY_INACTIVE = 6,
	/** Recovery mode */
	TRIMBLE_GPS_DISC_ACTIVITY_RECOVERY = 8,
	/** Calibration/control voltage */
	TRIMBLE_GPS_DISC_ACTIVITY_CALIBRATION = 9
};

#define R2D 57.2957795130823208767981548141051703

static bool construct()
{
	TRIMBLE_PARSER_init();
	return true;
}

static void cleanupParser(void * dummy)
{
	TRIMBLE_PARSER_disconnect();
}

static void mainloop()
{
	GPS_reset();

	while (true) {
		TRIMBLE_PARSER_connect();

		CLEANUP_PUSH(&cleanupParser, NULL);

		GPS_reset();

		while (true) {
			uint8_t buf[1024];
			size_t len = TRIMBLE_PARSER_getFrame(buf, sizeof buf);
			if (len == 0)
				break;
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

		CLEANUP_POP();
	}
}

#if 0
static void timeFrame(const uint8_t * buf)
{
	uint_fast32_t secondsOfWeek = TRIMBLE_GPS_tou32(buf + 1);
	uint_fast16_t week = TRIMBLE_GPS_tou16(buf + 5);
	int_fast16_t offset = TRIMBLE_GPS_toi16(buf + 7);
	bool testMode = !!(buf[9] & 0x10);
	bool hasOffset = !(buf[9] & 0x08);
	bool hasGpsTime = !(buf[9] & 0x04);
	bool isUTCTime = !!(buf[9] & 0x01);
	bool hasWeek = week != 1024;

	enum TRIMBLE_GPS_TIME_FLAGS flags;
	flags = (testMode ? 0 : TRIMBLE_GPS_TIME_FLAG_NON_TEST_MODE) |
		(hasGpsTime ? TRIMBLE_GPS_TIME_FLAG_HAS_TRIMBLE_GPS_TIME : 0) |
		(hasWeek ? TRIMBLE_GPS_TIME_FLAG_HAS_WEEK : 0) |
		(hasOffset ? TRIMBLE_GPS_TIME_FLAG_HAS_OFFSET : 0) |
		(isUTCTime ? TRIMBLE_GPS_TIME_FLAG_IS_UTC : 0);
	atomic_store_explicit(&TRIMBLE_GPS_timeFlags, flags, memory_order_relaxed);
#if 0
	enum TRIMBLE_GPS_TIME_STATUS stat = preGpsTime ? TRIMBLE_GPS_TIME_PRE : hasGpsTime ? TRIMBLE_GPS_TIME_FULL : TRIMBLE_GPS_TIME_NONE;

	if (!testMode) {
		//printf("isUTC: %d 0x%02x\n", isUTCTime, buf[9]);
		time_t unixtime = secondsOfWeek + week * 7 * 24 * 60 * 60 - offset +
			gps0;
		pthread_mutex_lock(&TRIMBLE_GPS_gps.mutex);
		TRIMBLE_GPS_gps.unixtime = unixtime;
		if (stat != TRIMBLE_GPS_gps.timeStatus)
			printf("GPS: timeStatus changed from %d to %d -> %s", TRIMBLE_GPS_gps.timeStatus, stat,
				ctime(&unixtime));
		TRIMBLE_GPS_gps.timeStatus = stat;
		if (hasOffset != TRIMBLE_GPS_gps.hasOffset)
			printf("GPS: hasOffset changed from %d to %d\n", TRIMBLE_GPS_gps.hasOffset, hasOffset);
		TRIMBLE_GPS_gps.hasOffset = hasOffset;
		pthread_mutex_unlock(&TRIMBLE_GPS_gps.mutex);
	}

	/*if (++TRIMBLE_GPS_gps.ctr == 10) {
		TRIMBLE_GPS_gps.ctr = 0;
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
	enum TRIMBLE_GPS_RECV_MODE recvMode = buf[1];
	enum TRIMBLE_GPS_DISC_MODE discMode = buf[2];
	//uint8_t selfSurveyProgress = buf[3];
	//uint32_t holdOverDuration = TRIMBLE_GPS_tou32(buf + 4);
	uint16_t critAlarms = ENDEC_tou16(buf + 8);
	uint16_t minorAlarms = ENDEC_tou16(buf + 10);
	enum TRIMBLE_GPS_DECODE_STATUS decodingStatus = buf[12];
	//enum TRIMBLE_GPS_DISC_ACTIVITY discActivity = buf[13];
	/*float ppsOffset = TRIMBLE_GPS_tofloat(buf + 16);
	float clockOffset = TRIMBLE_GPS_tofloat(buf + 20);
	uint32_t dacValue = TRIMBLE_GPS_tou32(buf + 24);
	float dacVoltage = TRIMBLE_GPS_tofloat(buf + 28);
	float temperature = TRIMBLE_GPS_tofloat(buf+ 32);*/
	double latitude = ENDEC_todouble(buf + 36) * R2D;
	double longitude = ENDEC_todouble(buf + 44) * R2D;
	double altitude = ENDEC_todouble(buf + 52);
	//float ppsQuantizationError = TRIMBLE_GPS_tofloat(buf + 60);

	bool selfSurvey = !!(minorAlarms & TRIMBLE_GPS_MIN_ALARM_SURVEY_IN_PROGRESS);
	bool inTestMode = !!(minorAlarms & TRIMBLE_GPS_MIN_ALARM_IN_TEST_MODE);

	/*NOC_printf("GPS Pos: %d %d ", recvMode == TRIMBLE_GPS_RECV_MODE_ODT_CLK,
		decodingStatus == TRIMBLE_GPS_DECODE_STATUS_DOING_FIXES);
	NOC_printf("LLA: %+6.2f %+6.2f %+6.2f\n", latitude, longitude,
		altitude);*/

	if (critAlarms || inTestMode || recvMode != TRIMBLE_GPS_RECV_MODE_ODT_CLK ||
		discMode != TRIMBLE_GPS_DISC_MODE_LOCKED) {
		/* unrecoverable or not automatically solvable by now */
		return;
	}

	if (selfSurvey || decodingStatus != TRIMBLE_GPS_DECODE_STATUS_DOING_FIXES ||
		discMode != TRIMBLE_GPS_DISC_MODE_LOCKED) {
		/* no position -> wait */
		return;
	}

	GPS_setPosition(latitude, longitude, altitude);

#if 0
	pthread_mutex_lock(&TRIMBLE_GPS_gps.mutex);
	if (antError != TRIMBLE_GPS_gps.antennaError)
		printf("GPS: antennaError changed from %d to %d\n", TRIMBLE_GPS_gps.antennaError, antError);
	TRIMBLE_GPS_gps.antennaError = antError;
	if (fixes != TRIMBLE_GPS_gps.hasPosition)
		printf("GPS: hasPosition changed from %d to %d -> %.2f %.2f\n",
			TRIMBLE_GPS_gps.hasPosition, fixes, latitude, longitude);
	TRIMBLE_GPS_gps.hasPosition = fixes;
	TRIMBLE_GPS_gps.latitude = latitude;
	TRIMBLE_GPS_gps.longitude = longitude;
	pthread_mutex_unlock(&TRIMBLE_GPS_gps.mutex);
#endif
}

