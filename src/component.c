/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _GNU_SOURCE
#include <component.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/** List of all components */
static struct Component * head = NULL, * tail = NULL;

static void stopUntil(struct Component * end);

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
void COMP_initAll()
{
	const struct Component * c;
	if (!silent)
		printf("Initializing components:");
	for (c = head; c; c = c->next) {
		if (c->construct)
			c->construct(c->data);
		if (!silent)
			printf(" %s", c->description);
	}
	if (!silent)
		putchar('\n');
}

/** Destruct all components. This is the opposite of COMP_initAll(). */
void COMP_destructAll()
{
	const struct Component * c;
	if (!silent)
		printf("Destructing components");
	for (c = tail; c; c = c->prev) {
		if (c->destruct)
			c->destruct();
		if (!silent)
			printf(" %s", c->description);
	}
	if (!silent)
		putchar('\n');
}

/** Start all components. If not all components could be started, the already
 *  started components are stopped.
 * @return true if all components were started. false otherwise.
 */
bool COMP_startAll()
{
	struct Component * c;
	if (!silent)
		printf("Starting components:");
	for (c = head; c; c = c->next) {
		c->stopped = false;
		if (!silent)
			printf(" %s", c->description);
		if (c->start && !c->start(c, c->data)) {
			/* failure: stop components again */
			if (!silent)
				printf(" [F]\n");
			stopUntil(c);
			return false;
		}
	}
	if (!silent)
		putchar('\n');
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
	if (!silent) {
		printf(" %s", c->description);
		fflush(stdout);
	}
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
	if (!silent)
		printf("Stopping components:");
	for (c = tail; c; c = c->prev) {
		stop(c, false);
		deferred |= !c->stopped;
	}
	if (!silent)
		putchar('\n');
	/* idea: if not all components could be stopped, try again while there is
	 * some progress, i.e. while at least one components could be stopped.
	 */
	bool progress = true;
	while (deferred && progress) {
		progress = false;
		deferred = false;
		if (!silent)
			printf("Stopping deferred components:");
		for (c = tail; c; c = c->prev) {
			if (!c->stopped) {
				stop(c, true);
				progress |= c->stopped;
				deferred |= !c->stopped;
			}
		}
		if (!silent)
			putchar('\n');
	}
	if (deferred && !silent) {
		/* there are still some components which could not be stopped */
		printf("The following components could not be stopped:");
		for (c = tail; c ; c = c->prev)
			if (!c->stopped)
				printf(" %s", c->description);
		putchar('\n');
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
			error(0, rc, "\n  Could not create thread [%s]", c->description);
			return false;
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
				printf("[F]");
			return false;
		}
	} else {
		pthread_cancel(c->thread);
		if (tryJoin(c) == ETIMEDOUT) {
			if (!silent)
				printf("[D]");
			return false;
		}
	}
	return true;
}
