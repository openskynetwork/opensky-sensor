/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include "util.h"
#include "log.h"

/** Component: Prefix */
static const char PFX[] = "UTIL";

/** Drop privileges by switching uid and gid to nobody */
void UTIL_dropPrivileges()
{
	if (getuid() != 0)
		return;

	uid_t u_nobody = 65534;
	gid_t g_nobody = 65534;

	long bufsz = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (bufsz == -1)
		bufsz = 16384;
	char * buffer = malloc(bufsz);
	if (buffer) {
		struct passwd pwd, * pwd_res;
		struct group grp, * grp_res;

		int pwd_err = getpwnam_r("nobody", &pwd, buffer, bufsz, &pwd_res);
		if (!pwd_err && pwd_res == &pwd) {
			u_nobody = pwd.pw_uid;
			g_nobody = pwd.pw_gid;
		}

		int grp_err = getgrnam_r("nobody", &grp, buffer, bufsz, &grp_res);
		if (!grp_err && grp_res == &grp)
			g_nobody = grp.gr_gid;

		free(buffer);
	}

	if (chdir("/tmp")) {} /* silence gcc */
	LOG_logf(LOG_LEVEL_INFO, PFX, "Dropping privileges to uid %u and "
		"gid %u\n", (unsigned)u_nobody, (unsigned)g_nobody);
	setgroups(0, NULL);
	if (setgid(g_nobody)) {}
	if (setuid(u_nobody)) {}
}

/** Get a random number.
 * @param n maximum number
 * @return returns a number between 0 and n
 */
uint32_t UTIL_randInt(uint32_t n)
{
	uint32_t limit = RAND_MAX - RAND_MAX % n;
	uint32_t rnd;

	do rnd = rand(); while (rnd >= limit);
	return rnd % n;
}

/** Wait for SIGINT and return then */
void UTIL_waitSigInt()
{
#ifdef CLEANUP_ROUTINES
	sigset_t set, oldset;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	int err = pthread_sigmask(SIG_BLOCK, &set, &oldset);
	if (err)
		LOG_errno(LOG_LEVEL_EMERG, PFX, "Could not install signal handler");

	int sig;
	err = sigwait(&set, &sig);
	if (err)
		LOG_errno2(LOG_LEVEL_EMERG, err, PFX, "Could not wait for signal");

	if (sig == SIGINT)
		LOG_log(LOG_LEVEL_INFO, PFX, "Caught SIGINT, cleaning up");
	else
		LOG_logf(LOG_LEVEL_ERROR, PFX, "Caught signal %d (%s), handling it "
			"as SIGINT", sig, strsignal(sig));
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
#else
	/* never return */
	while (true)
		pause();
#endif
}
