/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_COMPONENT_H
#define _HAVE_COMPONENT_H

#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Component {
	const char * description;

	void (*enroll)();

	bool (*construct)(void * data);
	void (*destruct)();

	void (*main)();

	bool (*start)(struct Component * c, void * data);
	bool (*stop)(struct Component * c, bool deferred);

	pthread_t thread;
	void * data;

	bool stopped;

	struct Component * next;
	struct Component * prev;
};

void COMP_setSilent(bool s);

void COMP_register(struct Component * comp, void * initData);

bool COMP_startThreaded(struct Component * comp, void * data);
bool COMP_stopThreaded(struct Component * comp, bool deferred);

bool COMP_initAll();
void COMP_destructAll();
bool COMP_startAll();
void COMP_stopAll();

#ifdef __cplusplus
}
#endif

#endif
