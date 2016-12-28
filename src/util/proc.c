/* Copyright (c) 2015-2016 OpenSky Network <contact@opensky-network.org> */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include "proc.h"
#include "log.h"

/** Component: Prefix */
static const char PFX[] = "PROC";

/** Print a command line which is about to be executed to stdout.
 * @param argv the whole argument vector where argv[0] is the executable.
 */
static void printCmdLine(char * const argv[])
{
	char * const * arg;

	char buf[1000];
	buf[0] = '\0';
	char * const end = buf + sizeof(buf);
	char * ptr = buf + 1;

	for (arg = argv; *arg && ptr && ptr != end; ++arg) {
		ptr[-1] = ' ';
		ptr = memccpy(ptr, *arg, '\0', end - ptr);
	}
	if (*arg && (!ptr || ptr == end))
		strcpy(&end[-4], "...");

	LOG_logf(LOG_LEVEL_INFO, PFX, "Executing%s", buf);
}

/** Fork from parent process.
 * @return true if this is child, false if its the parent (returns twice)
 */
bool PROC_fork()
{
	pid_t childPid = fork();
	if (childPid == -1) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "fork failed");
		return false;
	} else if (childPid) { /* parent */
		return false;
	} else { /* child */
		return true;
	}
}

/** Execute a command and don't return.
 * @param argv argument vector.
 */
void PROC_execAndFinalize(char * const argv[])
{
	close(STDIN_FILENO);
	openat(STDIN_FILENO, "/dev/null", O_RDONLY);

	/* TODO: if we don't want to have the output of the process, we need to
	 * close STDOUT and STDERR:
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	 * we could also open a file and redirect STDOUT and STDERR:
	openat(STDOUT_FILENO, "/tmp/log", O_WRONLY | O_CREAT | O_APPEND, 0600);
	dup2(STDOUT_FILENO, STDERR_FILENO);
	*/

	(void)umask(~0755);

	PROC_execRaw(argv);
	_Exit(EXIT_FAILURE);
}

/** Print command line and execute. This will replace the current process by
 * the new process. */
void PROC_execRaw(char * const argv[])
{
	printCmdLine(argv);

	char * envp[] = { NULL };
	int ret = execve(argv[0], argv, envp);
	if (ret != 0)
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not execute");
}

/** Escape the cgroup, so systemd won't kill us when stopping the daemon */
static inline void escape_cgroup()
{
#ifdef WITH_SYSTEMD
	pid_t pid = getpid();
	FILE * f = fopen("/sys/fs/cgroup/systemd/tasks", "a");
	if (!f) {
		LOG_errno(LOG_LEVEL_WARN, PFX, "Could not open cgroup/tasks for "
			"appending");
		return;
	}
	fprintf(f, "%u\n", (unsigned int)pid);
	fclose(f);
#endif
}

/** Fork parent process, daemonize child and execute.
 * @param argv argument vector
 */
void PROC_forkAndExec(char * const argv[])
{
	if (!PROC_fork())
		return; /* parent */
	/* daemonize and chdir to root */
	if (daemon(0, 1) < 0) {
		_Exit(EXIT_FAILURE);
	} else {
		escape_cgroup();
		PROC_execAndFinalize(argv);
	}
}

/** Execute a command and return.
 * @note If the parent process should not wait, you should PROC_fork() first
 *       and either exit or use PROC_execAndFinalize as last call.
 * @param argv argument vector.
 * @return true if call succeeded
 */
bool PROC_execAndReturn(char * const argv[])
{
	pid_t pid = fork();
	if (!pid)
		PROC_execAndFinalize(argv);

	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
