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
	printf("Initializing components:");
	for (c = head; c; c = c->next) {
		if (c->construct)
			c->construct(c->data);
		printf(" %s", c->description);
	}
	putchar('\n');
}

void COMP_destructAll()
{
	const struct Component * c;
	printf("Destructing components");
	for (c = tail; c; c = c->prev) {
		if (c->destruct)
			c->destruct();
		printf(" %s", c->description);
	}
	putchar('\n');
}

bool COMP_startAll()
{
	struct Component * c;
	printf("Starting components:");
	for (c = head; c; c = c->next) {
		printf(" %s", c->description);
		if (c->start && !c->start(c, c->data)) {
			printf(" [FAIL]\n");
			stopUntil(c);
			return false;
		}
	}
	putchar('\n');
	return true;
}

static void stop(struct Component * c)
{
	printf(" %s", c->description);
	if (c->stop)
		c->stop(c);
}

void COMP_stopAll()
{
	struct Component * c;
	printf("Stopping components:");
	for (c = tail; c; c = c->prev)
		stop(c);
	putchar('\n');
}

bool COMP_startThreaded(struct Component * c, void * data)
{
	if (c->main) {
		c->run = true;
		int rc = pthread_create(&c->thread, NULL, (void*(*)(void*))(c->main),
			data);
		if (rc) {
			error(0, rc, "\n  Could not create thread [%s]", c->description);
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
	printf("Stopping components:");
	for (c = end->prev; c; c = c->prev)
		stop(c);
	putchar('\n');
}
