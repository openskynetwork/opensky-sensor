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
#include <assert.h>
#include "component.h"
#include "log.h"
#include "cfgfile.h"
#include "util.h"
#include "list.h"

static const char PFX[] = "COMP";

enum COMPONENT_STATE {
	COMPONENT_STATE_REGISTERED,
	COMPONENT_STATE_INITIALIZED,
	COMPONENT_STATE_STARTED,
};

struct ComponentI {
	const struct Component * comp;
	pthread_t thread;
	enum COMPONENT_STATE state;
	struct LIST_LinkD list;
};

/** List of all components */
static struct LIST_ListD components = LIST_ListD_INIT(components);

static bool exists(const struct Component * comp);
static void stop(struct ComponentI * c, bool deferred);
static void destruct(struct ComponentI * ci);
static void stopUntil(struct ComponentI * end);
static void destructUntil(struct ComponentI * end);
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
	if (exists(comp)) {
		LOG_logf(LOG_LEVEL_EMERG, PFX, "Component %s is already registered",
			comp->description);
	}

	struct ComponentI * ci = malloc(sizeof *ci);
	if (ci == NULL)
		LOG_errno(LOG_LEVEL_EMERG, PFX, "malloc failed");
	ci->comp = comp;
	ci->state = COMPONENT_STATE_REGISTERED;
	LIST_push(&components, LIST_link(ci, list));

#if 0
	LOG_logf(LOG_LEVEL_INFO, PFX, "Registered Component %s with config: %d and "
		"onRegister: %d", comp->description, !!comp->config,
		!!comp->onRegister);
#endif
	if (comp->config)
		CFG_registerSection(comp->config);
	if (comp->onRegister)
		comp->onRegister();
}

void COMP_unregisterAll()
{
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&components, l, n) {
		struct ComponentI * ci = LIST_item(l, struct ComponentI, list);
		assert(ci->state == COMPONENT_STATE_REGISTERED);
		free(ci);
	}
	LIST_init(&components);
}

static bool exists(const struct Component * comp)
{
	struct ComponentI * ci;
	LIST_foreachItem(&components, ci, list)
		if (ci->comp == comp)
			return true;
	return false;
}

static void fixDependencies()
{
	struct ComponentI * ci;
	struct Component const * const * dp;
	LIST_foreachItem(&components, ci, list)
		for (dp = ci->comp->dependencies; *dp; ++dp)
			if (!exists(*dp))
				COMP_register(*dp);
}

static void checkEnabledDependencies()
{
	struct ComponentI * ci;
	struct Component const * const * dp;
	LIST_foreachItem(&components, ci, list)
		if (!ci->comp->enabled || *ci->comp->enabled)
			for (dp = ci->comp->dependencies; *dp; ++dp)
				if ((*dp)->enabled && !*(*dp)->enabled)
					LOG_logf(LOG_LEVEL_EMERG, PFX, "Component %s needs "
						"component %s, which is disabled",
						ci->comp->description, (*dp)->description);
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
	struct LIST_ListD newList = LIST_ListD_INIT(newList);

	LIST_concatenatePush(&newList, &components);

	while (!LIST_empty(&newList)) {
		struct LIST_LinkD * l, * n;
		LIST_foreachSafe(&newList, l, n) {
			struct ComponentI * ci = LIST_item(l, struct ComponentI, list);
			if (allDependencies(ci->comp)) {
				LIST_unlink(&newList, l);
				LIST_push(&components, l);
				break;
			}
		}
		if (l == LIST_end(&newList))
			LOG_log(LOG_LEVEL_EMERG, PFX, "Cycle in dependencies detected");
	}
}

void COMP_fixup()
{
	fixDependencies();
	sequentialize();
}

bool COMP_initAll()
{
	struct ComponentI * ci;

	checkEnabledDependencies();

	LIST_foreachItem(&components, ci, list) {
		const struct Component * c = ci->comp;

		assert(ci->state == COMPONENT_STATE_REGISTERED);

		if (c->enabled && !*c->enabled)
			continue;

		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Initialize %s", c->description);

		if (c->onConstruct && !c->onConstruct()) {
			LOG_logf(LOG_LEVEL_ERROR, PFX, "Failed to initialize %s",
				c->description);
			destructUntil(ci);
			return false;
		}

		ci->state = COMPONENT_STATE_INITIALIZED;
	}
	return true;
}

/** Destruct all components. This is the opposite of COMP_initAll(). */
void COMP_destructAll()
{
	struct ComponentI * ci;
	LIST_foreachItemRev(&components, ci, list)
		destruct(ci);
}

/** Start all components. If not all components could be started, the already
 *  started components are stopped.
 * @return true if all components were started. false otherwise.
 */
bool COMP_startAll()
{
	struct ComponentI * ci;
	LIST_foreachItem(&components, ci, list) {
		if (ci->state != COMPONENT_STATE_INITIALIZED)
			continue;

		const struct Component * c = ci->comp;

		if (c->start && !*c->start)
			continue;

		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Start %s", c->description);

		bool started = true;
		if (!c->onStart && c->main)
			started = startThreaded(ci);
		else if (c->onStart)
			started = c->onStart();

		if (!started) {
			LOG_logf(LOG_LEVEL_WARN, PFX, "Failed to start %s", c->description);
			stopUntil(ci);
			return false;
		}

		ci->state = COMPONENT_STATE_STARTED;
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

	if (ci->state != COMPONENT_STATE_STARTED)
		return;

	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Stopping %s", c->description);

	bool stopped;
	if (!c->onStop && c->main)
		stopped = stopThreaded(ci, deferred);
	else if (c->onStop)
		stopped = c->onStop(deferred);
	else
		stopped = true;

	if (stopped)
		ci->state = COMPONENT_STATE_INITIALIZED;
}

/** Stop components up to a specific component. This is useful, if starting
 * failed a specific component c. Then all components up to c (except c)
 * should be stopped in reverse order.
 * @param end the first component NOT to be stopped
 */
static void stopUntil(struct ComponentI * end)
{
	struct ComponentI * ci = end;
	bool deferred = false;
	LIST_foreachItemRev_continue(&components, ci, list) {
		stop(ci, false);
		deferred |= ci->state == COMPONENT_STATE_STARTED;
	}
	/* idea: if not all components could be stopped, try again while there is
	 * some progress, i.e. while at least one components could be stopped.
	 */
	bool progress = true;
	while (deferred && progress) {
		progress = false;
		deferred = false;
		ci = end;
		LIST_foreachItemRev_continue(&components, ci, list) {
			if (ci->state == COMPONENT_STATE_STARTED) {
				stop(ci, true);
				progress |= ci->state != COMPONENT_STATE_STARTED;
				deferred |= ci->state == COMPONENT_STATE_STARTED;
			}
		}
	}
	if (deferred) {
		/* there are still some components which could not be stopped */
		char buffer[1000];
		char * buf = buffer;
		char * const bend = buffer + sizeof(buffer) - 1;
		ci = end;
		LIST_foreachItemRev_continue(&components, ci, list) {
			if (ci->state == COMPONENT_STATE_STARTED) {
				size_t len = MIN(strlen(ci->comp->description), bend - buf);
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
	stopUntil(LIST_item(LIST_end(&components), struct ComponentI, list));
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

static void destruct(struct ComponentI * ci)
{
	assert(ci->state != COMPONENT_STATE_STARTED);

	if (ci->state != COMPONENT_STATE_INITIALIZED)
		return;

	const struct Component * c = ci->comp;
	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Destruct %s", c->description);
	if (c->onDestruct)
		c->onDestruct(c);

	ci->state = COMPONENT_STATE_REGISTERED;
}

static void destructUntil(struct ComponentI * end)
{
	struct ComponentI * ci = end;
	LIST_foreachItemRev_continue(&components, ci, list)
		destruct(ci);
}

void COMP_resetAll()
{
	struct ComponentI * ci;
	LIST_foreachItem(&components, ci, list)
		if (ci->comp->onReset)
			ci->comp->onReset();
}
