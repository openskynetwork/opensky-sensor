/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <component.h>
#include <log.h>
#include <stdlib.h>

static const char PFX[] = "COMP";

static struct Component * head = NULL, * tail = NULL;

static void stopUntil(struct Component * end);

static bool silent = false;

void COMP_setSilent(bool s)
{
	silent = s;
}

void COMP_register(struct Component * comp, void * initData)
{
	if (!comp->start && comp->main) {
		comp->start = &COMP_startThreaded;
		comp->stop = &COMP_stopThreaded;
	}

	comp->next = NULL;
	comp->data = initData;
	if (head == NULL)
		head = comp;
	else
		tail->next = comp;
	comp->prev = tail;
	tail = comp;
}

void COMP_initAll()
{
	const struct Component * c;
	for (c = head; c; c = c->next) {
		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Init %s", c->description);
		if (c->construct)
			c->construct(c->data);
	}
}

void COMP_destructAll()
{
	const struct Component * c;
	for (c = tail; c; c = c->prev) {
		if (!silent)
			LOG_logf(LOG_LEVEL_INFO, PFX, "Destruct %s", c->description);
		if (c->destruct)
			c->destruct();
	}
}

bool COMP_startAll()
{
	struct Component * c;
	for (c = head; c; c = c->next) {
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

static void stop(struct Component * c)
{
	if (!silent)
		LOG_logf(LOG_LEVEL_INFO, PFX, "Stopping %s", c->description);
	if (c->stop)
		c->stop(c);
}

void COMP_stopAll()
{
	struct Component * c;
	for (c = tail; c; c = c->prev)
		stop(c);
}

static void stopUntil(struct Component * end)
{
	struct Component * c;
	for (c = end->prev; c; c = c->prev)
		stop(c);
}

bool COMP_startThreaded(struct Component * c, void * data)
{
	if (c->main) {
		int rc = pthread_create(&c->thread, NULL, (void*(*)(void*))(c->main),
			data);
		if (rc) {
			LOG_errno2(LOG_LEVEL_WARN, rc, PFX, "Could not create thread for "
				"%s", c->description);
			return false;
		}
	}
	return true;
}

void COMP_stopThreaded(struct Component * c)
{
	pthread_cancel(c->thread);
	pthread_join(c->thread, NULL);
}
