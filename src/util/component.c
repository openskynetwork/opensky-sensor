/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

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

/** Component: Prefix */
static const char PFX[] = "COMP";

/** State of a component */
enum COMPONENT_STATE {
	/** Component is registered */
	COMPONENT_STATE_REGISTERED,
	/** Component is initialized */
	COMPONENT_STATE_INITIALIZED,
	/** Component is started */
	COMPONENT_STATE_STARTED,
};

/** Private data for components */
struct ComponentLink {
	/** Component */
	const struct Component * comp;
	/** Component state */
	enum COMPONENT_STATE state;
	/** Thread reference (if managed by the subsystem) */
	pthread_t thread;
	/** Link to siblings */
	struct LIST_LinkD list;
};

/** List of all components */
static struct LIST_ListD components = LIST_ListD_INIT(components);

static bool exists(const struct Component * comp);
static void stop(struct ComponentLink * c, bool deferred);
static void destruct(struct ComponentLink * ci);
static void stopUntil(struct ComponentLink * end);
static void destructUntil(struct ComponentLink * end);
static bool startThreaded(struct ComponentLink * ci);
static bool stopThreaded(const struct ComponentLink * ci, bool deferred);

/** Silence all outputs (useful for unit testing) */
static bool silent = false;

/** Set verbosity of the component management.
 * @param s true if the management should not print any information
 */
void COMP_setSilent(bool s)
{
	silent = s;
}

/** Register a component
 * @param comp component to be registered
 */
void COMP_register(const struct Component * comp)
{
	if (exists(comp)) {
		LOG_logf(LOG_LEVEL_EMERG, PFX, "Component %s is already registered",
			comp->description);
	}

	struct ComponentLink * ci = malloc(sizeof *ci);
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

/** Unregister all components
 * @note components must be stopped and deconstructed first
 */
void COMP_unregisterAll()
{
	struct LIST_LinkD * l, * n;
	LIST_foreachSafe(&components, l, n) {
		struct ComponentLink * ci = LIST_item(l, struct ComponentLink, list);
		assert(ci->state == COMPONENT_STATE_REGISTERED);
		free(ci);
	}
	LIST_init(&components);
}

/** Check if a component already exists
 * @param comp component to be tested
 * @return true if component already exists
 */
static bool exists(const struct Component * comp)
{
	struct ComponentLink * ci;
	LIST_foreachItem(&components, ci, list)
		if (ci->comp == comp)
			return true;
	return false;
}

/** Make sure, that all dependencies are registered */
static void fixDependencies()
{
	struct ComponentLink * ci;
	struct Component const * const * dp;
	LIST_foreachItem(&components, ci, list)
		for (dp = ci->comp->dependencies; *dp; ++dp)
			if (!exists(*dp))
				COMP_register(*dp);
}

/** Check if all dependencies are enabled
 * @note Will log and not return if check fails
 */
static void checkEnabledDependencies()
{
	struct ComponentLink * ci;
	struct Component const * const * dp;
	LIST_foreachItem(&components, ci, list)
		if (!ci->comp->enabled || *ci->comp->enabled)
			for (dp = ci->comp->dependencies; *dp; ++dp)
				if ((*dp)->enabled && !*(*dp)->enabled)
					LOG_logf(LOG_LEVEL_EMERG, PFX, "Component %s needs "
						"component %s, which is disabled",
						ci->comp->description, (*dp)->description);
}

/** Check if all dependencies of a component are members of the list
 * @param c component to be tested
 * @return true if all dependencies of c are registered
 */
static bool allDependencies(const struct Component * c)
{
	struct Component const * const * dp;
	for (dp = c->dependencies; *dp; ++dp)
		if (!exists(*dp))
			return false;
	return true;
}

/** Sequentialize the list of components, such that all dependencies are
 * constructed/started before they are needed.
 * @note this function will log and not return if there are dependency cycles
 */
static void sequentialize()
{
	/* create a temp list and move all members of the old list to it */
	struct LIST_ListD tmpList = LIST_ListD_INIT(tmpList);
	LIST_concatenatePush(&tmpList, &components);

	while (!LIST_empty(&tmpList)) {
		/* there is at least one more component to be added to the list */
		struct LIST_LinkD * l;
		LIST_foreach(&tmpList, l) {
			struct ComponentLink * ci = LIST_item(l, struct ComponentLink,
				list);
			if (allDependencies(ci->comp)) {
				/* all dependencies are met -> move the component from the temp
				 * list to the permament list */
				LIST_unlink(&tmpList, l);
				LIST_push(&components, l);
				break; /* begin from start again */
			}
		}
		if (l == LIST_end(&tmpList)) {
			/* nothing done this time -> cycle detected */
			LOG_log(LOG_LEVEL_EMERG, PFX, "Cycle in dependencies detected");
		}
	}
}

/** After registering all components, call this function to fix the dependencies
 */
void COMP_fixup()
{
	fixDependencies();
	sequentialize();
}

/** Initialize all components
 * @return true if all components could be initialized successfully
 */
bool COMP_initAll()
{
	struct ComponentLink * ci;

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
	struct ComponentLink * ci;
	LIST_foreachItemRev(&components, ci, list)
		destruct(ci);
}

/** Start all components. If not all components could be started, the already
 *  started components are stopped again.
 * @return true if all components were started successfully
 */
bool COMP_startAll()
{
	struct ComponentLink * ci;
	LIST_foreachItem(&components, ci, list) {
		if (ci->state != COMPONENT_STATE_INITIALIZED)
			continue;

		const struct Component * c = ci->comp;

		if (c->start && !*c->start)
			continue;

		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Start %s", c->description);

		bool started = true;
		if (!c->onStart && c->main) /* we shall create and manage a thread */
			started = startThreaded(ci);
		else if (c->onStart) /* the component manages itself */
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
 * @param ci component to be stopped
 * @param deferred true if the stop was deferred, i.e. it is not the first time
 *  the component was tried to be stopped. This is passed to the stop function
 *  of the component.
 */
static void stop(struct ComponentLink * ci, bool deferred)
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
 * failed a specific component. Then all components up to that (except itself)
 * should be stopped in reverse order.
 * @param end the first component NOT to be stopped
 */
static void stopUntil(struct ComponentLink * end)
{
	struct ComponentLink * ci = end;
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
	stopUntil(LIST_item(LIST_end(&components), struct ComponentLink, list));
}

/** Start a threaded component.
 * @param ci component to be started
 * @return true if starting the component was successful
 */
static bool startThreaded(struct ComponentLink * ci)
{
	int rc = pthread_create(&ci->thread, NULL,
		(void*(*)(void*))(ci->comp->main), NULL);
	if (rc) {
		LOG_errno2(LOG_LEVEL_WARN, rc, PFX, "Could not create thread for "
			"component %s", ci->comp->description);
		return false;
	} else {
#ifdef HAVE_PTHREAD_SETNAME_NP
		/* set its name for debugging purposes */
		pthread_setname_np(ci->thread, ci->comp->description);
#endif
	}
	return true;
}

#ifdef HAVE_PTHREAD_TIMEDJOIN_NP
/** Try to join a thread with a timeout of one second.
 * @param ci threaded component to be stopped
 * @return the value of pthread_timedjoin_np(), especially 0 on success and
 *  ETIMEDOUT on timeout
 */
static int tryJoin(const struct ComponentLink * ci)
{
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += 1;
	return pthread_timedjoin_np(ci->thread, NULL, &ts);
}
#endif

/** Stop a threaded component.
 * @param ci component to be stopped
 * @param deferred true if we're not stopping the first time. This will prevent
 *  pthread_cancel to be called again.
 * @return true if stopping succeeded, false on timeout (one second)
 */
static bool stopThreaded(const struct ComponentLink * ci, bool deferred)
{
	if (deferred) {
#ifdef HAVE_PTHREAD_TIMEDJOIN_NP
		if (tryJoin(ci) != 0) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined", ci->comp->description);
			return false;
		}
#endif
		pthread_join(ci->thread, NULL);
	} else {
		pthread_cancel(ci->thread);
#if HAVE_PTHREAD_TIMEDJOIN_NP
		if (tryJoin(ci) != 0) {
			if (!silent)
				LOG_logf(LOG_LEVEL_WARN, PFX, "Component %s could not be "
					"joined, deferring", ci->comp->description);
			return false;
		}
#else
		return false;
#endif
	}
	return true;
}

/** Destruct a component.
 * @param ci component to be destructed
 */
static void destruct(struct ComponentLink * ci)
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

/** Destruct all components up to (excluding) a given component.
 * @param end the first component NOT to be destructed
 */
static void destructUntil(struct ComponentLink * end)
{
	struct ComponentLink * ci = end;
	LIST_foreachItemRev_continue(&components, ci, list)
		destruct(ci);
}

void COMP_resetAll()
{
	struct ComponentLink * ci;
	LIST_foreachItem(&components, ci, list)
		if (ci->comp->onReset)
			ci->comp->onReset();
}
