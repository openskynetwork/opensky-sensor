/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_COMPONENT_H
#define _HAVE_COMPONENT_H

#include <stdbool.h>
#include <pthread.h>
#include "cfgfile.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Component {
	const char * description;

	void (*onRegister)();

	bool (*onConstruct)();
	void (*onDestruct)();

	void (*main)();

	bool (*onStart)();
	bool (*onStop)(bool deferred);

	bool * enabled;
	bool * start;

	const struct CFG_Section * config;

	const struct Component * dependencies[];
};


void COMP_setSilent(bool s);

void COMP_register(const struct Component * comp);

void COMP_fixup();
bool COMP_initAll();
void COMP_destructAll();
bool COMP_startAll();
void COMP_stopAll();

#ifdef __cplusplus
}
#endif

#endif
