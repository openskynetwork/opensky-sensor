/* Copyright (c) 2015-2017 OpenSky Network <contact@opensky-network.org> */

#ifndef _HAVE_COMPONENT_H
#define _HAVE_COMPONENT_H

#include <stdbool.h>
#include <pthread.h>
#include "cfgfile.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Component Descriptor */
struct Component {
	/** Component name */
	const char * description;

	/** Callback: called upon registering the component */
	void (*onRegister)();

	/** Callback: called upon construction
	 * @return true if component was constructed successfully
	 */
	bool (*onConstruct)();
	/** Callback: called upon destruction */
	void (*onDestruct)();

	/** Callback: main loop */
	void (*main)();

	/** Callback: called upon start
	 * @return true if component was started successfully
	 */
	bool (*onStart)();
	/** Callback: called upon stop
	 * @param deferred true if stopping failed earlier and was deferred
	 * @return true if component was stopped successfully
	 */
	bool (*onStop)(bool deferred);

	/** Callback: reset statistics */
	void (*onReset)();

	bool * enabled;
	/** pointer to boolean, indicating if this component should be started */
	bool * start;

	/** Component configuration descriptor */
	const struct CFG_Section * config;

	const struct Component * dependencies[];
};

void COMP_setSilent(bool s);

void COMP_register(const struct Component * comp);
void COMP_unregisterAll();

void COMP_fixup();
bool COMP_initAll();
void COMP_destructAll();
bool COMP_startAll();
void COMP_stopAll();
void COMP_resetAll();

#ifdef __cplusplus
}
#endif

#endif
