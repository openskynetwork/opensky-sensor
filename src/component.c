#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <component.h>
#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static struct Component * head = NULL, * tail = NULL;

static void stopUntil(struct Component * end);

void COMP_register(struct Component * comp, void * initData)
{
	if (!comp->start) {
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
		c->construct(c->data);
		printf("Initalized component '%s'\n", c->description);
	}
}

void COMP_destructAll()
{
	const struct Component * c;
	for (c = tail; c; c = c->prev)
		c->destruct();
}

bool COMP_startAll()
{
	struct Component * c;
	for (c = head; c; c = c->next) {
		if (!c->start(c, c->data)) {
			printf("Could not start component '%s'\n", c->description);
			stopUntil(c);
			return false;
		} else {
			printf("Started component '%s'\n", c->description);
		}
	}
	return true;
}

static void stop(struct Component * c)
{
	if (c->stop)
		c->stop(c);
	printf("Stopped component '%s'\n", c->description);
}

void COMP_stopAll()
{
	struct Component * c;
	for (c = tail; c; c = c->prev)
		stop(c);
}

bool COMP_startThreaded(struct Component * c, void * data)
{
	if (c->main) {
		c->run = true;
		int rc = pthread_create(&c->thread, NULL, (void*(*)(void*))(c->main),
			data);
		if (rc) {
			error(0, rc, "Could not create thread [%s]", c->description);
			return false;
		}
	}
	return true;
}

void COMP_stopThreaded(struct Component * c)
{
	c->run = false;
	pthread_cancel(c->thread);
	pthread_join(c->thread, NULL);
}

static void stopUntil(struct Component * end)
{
	struct Component * c;
	for (c = end->prev; c; c = c->prev)
		stop(c);
}
