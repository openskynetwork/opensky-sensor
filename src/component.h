#ifndef _HAVE_COMPONENT_H
#define _HAVE_COMPONENT_H

#include <stdbool.h>
#include <pthread.h>

struct Component {
	const char * description;

	void (*construct)(void * data);
	void (*destruct)();

	void (*main)();

	bool (*start)(struct Component * c, void * data);
	void (*stop)(struct Component * c);

	pthread_t thread;
	void * data;

	struct Component * next;
	struct Component * prev;
};

void COMP_setSilent(bool s);

void COMP_register(struct Component * comp, void * initData);

bool COMP_startThreaded(struct Component * comp, void * data);
void COMP_stopThreaded(struct Component * comp);

void COMP_initAll();
void COMP_destructAll();
bool COMP_startAll();
void COMP_stopAll();

#endif
