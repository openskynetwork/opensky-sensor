/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <component.h>
#include <log.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <util.h>

static const char PFX[] = "COMP";

/** List of all components */
static struct Component * head = NULL, * tail = NULL;

static void stop(struct Component * c, bool deferred);
static void destruct(const struct Component * c);
static void stopUntil(struct Component * end);
static void destructUntil(const struct Component * end);

/** Silence all outputs (useful for unit testing) */
static bool silent = false;

/** Set verbosity of the component management.
 * \param s true if the management should not print any information
 */
void COMP_setSilent(bool s)
{
	silent = s;
}

/** Register a component
 * \param comp component
 * \param initData initial data, passed to the components' start function
 */
void COMP_register(struct Component * comp, void * initData)
{
	/* local fixups */
	if (!comp->start && comp->main) {
		comp->start = &COMP_startThreaded;
		comp->stop = &COMP_stopThreaded;
	}

	/* append to list */
	comp->next = NULL;
	comp->data = initData;
	if (head == NULL)
		head = comp;
	else
		tail->next = comp;
	comp->prev = tail;
	tail = comp;
}

/** Initialize all components */
bool COMP_initAll()
{
	const struct Component * c;
	for (c = head; c; c = c->next) {
		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Initialize %s", c->description);
		if (c->construct && !c->construct(c->data)) {
			LOG_logf(LOG_LEVEL_ERROR, PFX, "Failed to initialize %s",
				c->description);
			destructUntil(c);
			return false;
		}
	}
	return true;
}

/** Destruct all components. This is the opposite of COMP_initAll(). */
void COMP_destructAll()
{
	const struct Component * c;
	for (c = tail; c; c = c->prev)
		destruct(c);
}

/** Start all components. If not all components could be started, the already
 *  started components are stopped.
 * @return true if all components were started. false otherwise.
 */
bool COMP_startAll()
{
	struct Component * c;
	for (c = head; c; c = c->next) {
		c->stopped = false;
		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Start %s", c->description);
		if (c->start && !c->start(c, c->data)) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Failed to start %s", c->description);
			stopUntil(c);
			return false;
		}
	}
	return true;
}

/** Stop a component.
 * @param c component to be stopped
 * @param deferred true if the stop was deferred, i.e. it is not the first time
 *  the component was tried to be stopped. This is passed to the stop function
 *  of the component.
 */
static void stop(struct Component * c, bool deferred)
{
	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Stopping %s", c->description);
	if (c->stop) {
		c->stopped = c->stop(c, deferred);
	} else {
		c->stopped = true;
	}
}

/** Stop all components in reverse order, starting at a specific component.
 * @param tail first component to be stopped.
 */
void stopAll(struct Component * tail)
{
	struct Component * c;
	bool deferred = false;
	for (c = tail; c; c = c->prev) {
		stop(c, false);
		deferred |= !c->stopped;
	}
	/* idea: if not all components could be stopped, try again while there is
	 * some progress, i.e. while at least one components could be stopped.
	 */
	bool progress = true;
	while (deferred && progress) {
		progress = false;
		deferred = false;
		for (c = tail; c; c = c->prev) {
			if (!c->stopped) {
				stop(c, true);
				progress |= c->stopped;
				deferred |= !c->stopped;
			}
		}
	}
	if (deferred) {
		/* there are still some components which could not be stopped */
		char buffer[1000];
		char * buf = buffer;
		char * const end = buffer + sizeof(buffer) - 1;
		for (c = tail; c && buf < end; c = c->prev) {
			if (!c->stopped) {
				size_t len = MIN(strlen(c->description), end - buf);
				memcpy(buf, c->description, len);
				buf += len;
				*(buf++) = ' ';
			}
		}
		if (buf != buffer)
			buf[-1] = 0;
		else
			*buf = 0;
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Some components could not be stopped: "
			"%s", buffer);
	}
}

/** Stop all components. */
void COMP_stopAll()
{
	stopAll(tail);
}

/** Stop components up to a specific component. This is useful, if starting
 * failed a specific component c. Then all components up to c (except c)
 * should be stopped in reverse order.
 * @param end the first component NOT to be stopped
 */
static void stopUntil(struct Component * end)
{
	stopAll(end->prev);
}

/** Start a component using a thread.
 * @param c component to be started
 * @param data initial data
 * @return true if starting the component was successful
 */
bool COMP_startThreaded(struct Component * c, void * data)
{
	if (c->main) {
		int rc = pthread_create(&c->thread, NULL, (void*(*)(void*))(c->main),
			data);
		if (rc) {
			LOG_errno2(LOG_LEVEL_WARN, rc, PFX, "Could not create thread for "
				"%s", c->description);
			return false;
		} else {
			pthread_setname_np(c->thread, c->description);
		}
	}
	return true;
}

/** Try to join a thread with a timeout of one second.
 * @param c threaded component to be stopped
 * @return the value of pthread_timedjoin_np()
 */
static int tryJoin(struct Component * c)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	return pthread_timedjoin_np(c->thread, NULL, &ts);
}

/** Stop a threaded component.
 * @param c component to be stopped
 * @param deferred true if we're not stopping the first time. This will prevent
 *  pthread_cancel to be called again.
 * @return true if stopping succeeded, false on timeout (one second)
 */
bool COMP_stopThreaded(struct Component * c, bool deferred)
{
	if (deferred) {
		if (tryJoin(c) != 0) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined", c->description);
			return false;
		}
	} else {
		pthread_cancel(c->thread);
		if (tryJoin(c) == ETIMEDOUT) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined, deferring", c->description);
			return false;
		}
	}
	return true;
}

static void destruct(const struct Component * c)
{
	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Destruct %s", c->description);
	if (c->destruct)
		c->destruct(c);
}

static void destructUntil(const struct Component * end)
{
	const struct Component * c;
	for (c = end->prev; c; c = c->prev)
		destruct(c);
}
