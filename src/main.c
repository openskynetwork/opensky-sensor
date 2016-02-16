/* Copyright (c) 2015-2016 SeRo Systems <contact@sero-systems.de> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gpio.h>
#include <watchdog.h>
#include <fpga.h>
#include <stdlib.h>
#include <stdio.h>
#include <buffer.h>
#include <network.h>
#include <cfgfile.h>
#include <statistics.h>
#include <tb.h>
#include <recv.h>
#include <relay.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <error.h>

#if defined(DEVELOPMENT) && !defined(SYSCONFDIR)
#define SYSCONFDIR "."
#endif

static void sigint(int sig);

bool run;
static pthread_mutex_t sigmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t sigcond = PTHREAD_COND_INITIALIZER;

int main(int argc, char * argv[])
{
	/* force flushing of stdout and stderr on newline */
	setlinebuf(stdout);

#ifdef STANDALONE
	bool bbwhite = true;
	if (argc == 2) {
		if (!strcmp(argv[1], "--black")) {
			bbwhite = false;
		} else {
			error(EXIT_FAILURE, 0, "Usage: %s [--black]", argv[0]);
		}
	}
#endif

	/* read & check configuration */
	CFG_read(SYSCONFDIR "/openskyd.conf");

#ifdef STANDALONE
	if (CFG_config.wd.enabled || CFG_config.fpga.configure)
		COMP_register(&GPIO_comp, NULL);
#endif
	if (CFG_config.stats.enabled)
		COMP_register(&STAT_comp, NULL);
#ifdef STANDALONE
	if (CFG_config.wd.enabled)
		COMP_register(&WD_comp, NULL);
#endif
#ifdef STANDALONE
	if (CFG_config.fpga.configure)
		COMP_register(&FPGA_comp, &bbwhite);
#endif
	COMP_register(&BUF_comp, NULL);
	COMP_register(&NET_comp, NULL);
	COMP_register(&TB_comp, argv);
	COMP_register(&RECV_comp, NULL);
	COMP_register(&RELAY_comp, NULL);

	COMP_initAll();

	if (!COMP_startAll())
		return EXIT_FAILURE;

	run = true;
	pthread_mutex_lock(&sigmutex);
#ifdef CLEANUP
	signal(SIGINT, &sigint);
#endif
	while (run)
		pthread_cond_wait(&sigcond, &sigmutex);
	signal(SIGINT, SIG_DFL);
	pthread_mutex_unlock(&sigmutex);

	COMP_stopAll();
	COMP_destructAll();

	return EXIT_SUCCESS;
}

__attribute__((unused))
static void sigint(int sig)
{
	pthread_mutex_lock(&sigmutex);
	run = false;
	pthread_cond_broadcast(&sigcond);
	pthread_mutex_unlock(&sigmutex);
}
