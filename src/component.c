/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <util.h>
#include "component.h"
#include "log.h"

#include <stdio.h>
#include <assert.h>

static const char PFX[] = "COMP";

struct ComponentI {
	const struct Component * comp;
	pthread_t thread;
	bool stopped;
	struct ComponentI * next;
	struct ComponentI * prev;
};

/** List of all components */
static struct ComponentI * head = NULL, * tail = NULL;

static void stop(struct ComponentI * c, bool deferred);
static void destruct(const struct Component * c);
static void stopUntil(struct ComponentI * end);
static void destructUntil(const struct ComponentI * end);
static bool startThreaded(struct ComponentI * ci);
static bool stopThreaded(const struct ComponentI * ci, bool deferred);

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
void COMP_register(const struct Component * comp)
{
	struct ComponentI * ci = malloc(sizeof *ci);
	if (ci == NULL)
		LOG_errno(LOG_LEVEL_EMERG, "malloc failed");
	ci->comp = comp;
	ci->stopped = true;
	ci->next = NULL;
	if (head == NULL)
		head = ci;
	else
		tail->next = ci;
	ci->prev = tail;
	tail = ci;
}

static bool exists(const struct Component * comp)
{
	struct ComponentI * ci;
	for (ci = head; ci; ci = ci->next)
		if (ci->comp == comp)
			return true;
	return false;
}

static void fixDependencies()
{
	struct ComponentI * ci;
	struct Component const * const * dp;
	for (ci = head; ci; ci = ci->next)
		for (dp = ci->comp->dependencies; *dp; ++dp)
			if (!exists(*dp))
				COMP_register(*dp);
}

static bool allDependencies(const struct Component * c)
{
	struct Component const * const * dp;
	for (dp = c->dependencies; *dp; ++dp)
		if (!exists(*dp))
			return false;
	return true;
}

static void sequentialize()
{
	struct ComponentI * oldHead = head;

	head = tail = NULL;

	while (oldHead) {
		struct ComponentI * ci;
		for (ci = oldHead; ci; ci = ci->next) {
			if (allDependencies(ci->comp)) {
				if (ci == oldHead)
					oldHead = oldHead->next;

				if (ci->prev)
					ci->prev->next = ci->next;
				if (ci->next)
					ci->next->prev = ci->prev;

				ci->next = NULL;
				if (head == NULL)
					head = ci;
				else
					tail->next = ci;
				ci->prev = tail;
				tail = ci;
				break;
			}
		}
		if (ci == NULL)
			LOG_log(LOG_LEVEL_EMERG, PFX, "Cycle in dependencies detected");
	}
}

bool COMP_initAll()
{
	fixDependencies();
	sequentialize();

	const struct ComponentI * ci;
	for (ci = head; ci; ci = ci->next) {
		const struct Component * c = ci->comp;
		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Initialize %s", c->description);

		if (c->construct && !c->construct()) {
			LOG_logf(LOG_LEVEL_ERROR, PFX, "Failed to initialize %s",
				c->description);
			destructUntil(ci);
			return false;
		}
	}
	return true;
}

/** Destruct all components. This is the opposite of COMP_initAll(). */
void COMP_destructAll()
{
	const struct ComponentI * ci;
	for (ci = tail; ci; ci = ci->prev)
		destruct(ci->comp);
}

/** Start all components. If not all components could be started, the already
 *  started components are stopped.
 * @return true if all components were started. false otherwise.
 */
bool COMP_startAll()
{
	struct ComponentI * ci;
	for (ci = head; ci; ci = ci->next) {
		const struct Component * c = ci->comp;

		ci->stopped = false;

		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Start %s", c->description);

		bool started = true;
		if (!c->start && c->main)
			started = startThreaded(ci);
		else if (c->start)
			started = c->start();

		if (!started) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Failed to start %s", c->description);
			stopUntil(ci);
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
static void stop(struct ComponentI * ci, bool deferred)
{
	const struct Component * c = ci->comp;

	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Stopping %s", c->description);

	ci->stopped = true;
	if (!c->stop && c->main)
		ci->stopped = stopThreaded(ci, deferred);
	else if (c->stop)
		ci->stopped = c->stop(deferred);
}

/** Stop all components in reverse order, starting at a specific component.
 * @param tail first component to be stopped.
 */
void stopAll(struct ComponentI * tail)
{
	struct ComponentI * ci;
	bool deferred = false;
	for (ci = tail; ci; ci = ci->prev) {
		stop(ci, false);
		deferred |= !ci->stopped;
	}
	/* idea: if not all components could be stopped, try again while there is
	 * some progress, i.e. while at least one components could be stopped.
	 */
	bool progress = true;
	while (deferred && progress) {
		progress = false;
		deferred = false;
		for (ci = tail; ci; ci = ci->prev) {
			if (!ci->stopped) {
				stop(ci, true);
				progress |= ci->stopped;
				deferred |= !ci->stopped;
			}
		}
	}
	if (deferred) {
		/* there are still some components which could not be stopped */
		char buffer[1000];
		char * buf = buffer;
		char * const end = buffer + sizeof(buffer) - 1;
		for (ci = tail; ci && buf < end; ci = ci->prev) {
			if (!ci->stopped) {
				size_t len = MIN(strlen(ci->comp->description), end - buf);
				memcpy(buf, ci->comp->description, len);
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
static void stopUntil(struct ComponentI * end)
{
	stopAll(end->prev);
}

/** Start a component using a thread.
 * @param c component to be started
 * @param data initial data
 * @return true if starting the component was successful
 */
static bool startThreaded(struct ComponentI * ci)
{
	int rc = pthread_create(&ci->thread, NULL,
		(void*(*)(void*))(ci->comp->main), NULL);
	if (rc) {
		LOG_errno2(LOG_LEVEL_WARN, rc, PFX, "Could not create thread for "
			"component %s", ci->comp->description);
		return false;
	} else {
		pthread_setname_np(ci->thread, ci->comp->description);
	}
	return true;
}

/** Try to join a thread with a timeout of one second.
 * @param c threaded component to be stopped
 * @return the value of pthread_timedjoin_np()
 */
static int tryJoin(const struct ComponentI * ci)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	return pthread_timedjoin_np(ci->thread, NULL, &ts);
}

/** Stop a threaded component.
 * @param c component to be stopped
 * @param deferred true if we're not stopping the first time. This will prevent
 *  pthread_cancel to be called again.
 * @return true if stopping succeeded, false on timeout (one second)
 */
static bool stopThreaded(const struct ComponentI * ci, bool deferred)
{
	if (deferred) {
		if (tryJoin(ci) != 0) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined", ci->comp->description);
			return false;
		}
	} else {
		pthread_cancel(ci->thread);
		if (tryJoin(ci) == ETIMEDOUT) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined, deferring", ci->comp->description);
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

static void destructUntil(const struct ComponentI * end)
{
	const struct ComponentI * ci;
	for (ci = end->prev; ci; ci = ci->prev)
		destruct(ci->comp);
}
