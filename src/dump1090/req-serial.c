/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "core/serial.h"
#include "core/beast.h"
#include "core/network.h"
#include "core/tb.h"
#include "util/threads.h"
#include "util/cfgfile.h"
#include "util/endec.h"
#include "util/log.h"
#include "req-serial.h"

static const char PFX[] = "SERIAL";

#if (defined(ECLIPSE) || defined(LOCAL_FILES)) && !defined(LOCALSTATEDIR)
#define LOCALSTATEDIR "var"
#endif

static bool construct();
static void destruct();
static bool sendSerialRequest();

static pthread_mutex_t serialMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t serialReqCond = PTHREAD_COND_INITIALIZER;
static bool hasSerial;
static uint32_t serialNumber;

static const struct CFG_Section cfg =
{
	.name = "Device",
	.n_opt = 1,
	.options = {
		{
			.name = "serial",
			.type = CFG_VALUE_TYPE_INT,
			.var = &serialNumber,
			.given = &hasSerial,
		}
	}
};

const struct Component SERIAL_comp =
{
	.description = PFX,

	.onConstruct = &construct,
	.onDestruct = &destruct,

	.config = &cfg,

	.dependencies = { &TB_comp, NULL }
};

static void cleanup(void * dummy)
{
	pthread_mutex_unlock(&serialMutex);
}

enum SERIAL_RETURN SERIAL_getSerial(uint32_t * serial)
{
	enum SERIAL_RETURN rc = SERIAL_RETURN_SUCCESS;
	pthread_mutex_lock(&serialMutex);
	CLEANUP_PUSH(&cleanup, NULL);
	if (!hasSerial) {
		LOG_log(LOG_LEVEL_INFO, PFX, "Requesting new serial number");
		if (!sendSerialRequest()) {
			rc = SERIAL_RETURN_FAIL_NET;
			goto cleanup;
		}

		/* Little hack: we could wait 10 seconds and print our message
		 * but then we would also have to wait up to 10 seconds to see any
		 * reconnects. Making this a bit cleaner would also raise complexity,
		 * but it should be a rare condition here.
		 */

		struct timespec ts;
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += 2;

		uint32_t rounds = 1;
		while (!hasSerial) {
			int r = pthread_cond_timedwait(&serialReqCond, &serialMutex, &ts);
			if (r == ETIMEDOUT) {
				if (!NET_checkConnected()) {
					rc = SERIAL_RETURN_FAIL_NET;
					goto cleanup;
				}
				if (rounds == 30) {
					LOG_log(LOG_LEVEL_WARN, PFX, "No serial number after "
							"one minute, reconnecting");
					NET_forceDisconnect();
					rc = SERIAL_RETURN_FAIL_NET;
					goto cleanup;
				} else {
					if (rounds % 5 == 0)
						LOG_logf(LOG_LEVEL_WARN, PFX, "No serial number after "
							"%" PRIu32 " seconds, keep waiting", rounds * 2);
					++rounds;
					ts.tv_sec += 2;
				}
			} else if (r) {
				LOG_errno2(LOG_LEVEL_EMERG, r, PFX, "pthread_cond_timedwait "
					"failed");
			}
		}

		LOG_logf(LOG_LEVEL_INFO, PFX, "Got a new serial number: %" PRIu32,
			serialNumber);

		/* got a new serial number -> save it */
		if (!CFG_writeSection(LOCALSTATEDIR "/conf.d/10-serial.conf", &cfg)) {
			LOG_log(LOG_LEVEL_EMERG, PFX, "Could not write serial number, "
				"refusing to work");
		}
	}
	*serial = serialNumber;
cleanup:
	CLEANUP_POP();
	return rc;
}

static bool sendSerialRequest()
{
	uint8_t buf[2] = { BEAST_SYNC, BEAST_TYPE_SERIAL_REQ };
	return NET_send(buf, sizeof buf);
}

static void serialResponse(const uint8_t * payload)
{
	pthread_mutex_lock(&serialMutex);
	serialNumber = ENDEC_tou32(payload);
	hasSerial = true;
	pthread_cond_broadcast(&serialReqCond);
	pthread_mutex_unlock(&serialMutex);
}

static bool construct()
{
	TB_register(TB_PACKET_TYPE_SERIAL_RES, 4, &serialResponse);
	return true;
}

static void destruct()
{
	TB_unregister(TB_PACKET_TYPE_SERIAL_RES);
}
