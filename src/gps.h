#ifndef _HAVE_GPS_H
#define _HAVE_GPS_H

#include <component.h>
#include <stdint.h>

extern struct Component GPS_comp;

enum GPS_TIME_STATUS {
	GPS_TIME_NONE,
	GPS_TIME_PRE,
	GPS_TIME_FULL,
};

struct GPS {
	pthread_mutex_t mutex;
	time_t unixtime;
	double latitude;
	double longitude;
	enum GPS_TIME_STATUS timeStatus;
	bool hasOffset;
	bool hasPosition;
	bool antennaError;

	uint32_t ctr;
};

extern struct GPS GPS_gps;

#endif
