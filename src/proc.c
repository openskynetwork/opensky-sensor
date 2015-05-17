#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <proc.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

/** Print a command line which is about to be executed to stdout.
 * \param argv the whole argument vector where argv[0] is the executable.
 */
static void printCmdLine(char * argv[])
{
	char * const * arg;

	printf("PROC: Executing ");

	for (arg = argv; *arg; ++arg)
		printf("%s%c", *arg, arg[1] ? ' ' : '\n');
}

/** Fork from parent process.
 * \return true if this is child, false if its the parent (returns twice)
 */
bool PROC_fork()
{
	pid_t childPid = fork();
	if (childPid == -1) {
		error(0, errno, "fork failed");
		return false;
	} else if (childPid) { /* parent */
		return false;
	} else { /* child */
		return true;
	}
}

/** Execute a command and don't return.
 * \param argv argument vector.
 */
void PROC_execAndFinalize(char * argv[])
{
	close(STDIN_FILENO);
	/*close(STDOUT_FILENO);
	close(STDERR_FILENO);*/
	openat(STDIN_FILENO, "/dev/null", O_RDONLY);
	/*openat(STDOUT_FILENO, "/tmp/log", O_WRONLY | O_CREAT | O_APPEND, 0600);
	dup2(STDOUT_FILENO, STDERR_FILENO);*/

	(void)umask(~0755);

	PROC_execRaw(argv);
	_exit(EXIT_FAILURE);
}

/** Print command line and execute. This will replace the current process by
 * the new process. */
void PROC_execRaw(char * argv[])
{
	printCmdLine(argv);

	char * envp[] = { NULL };
	int ret = execve(argv[0], argv, envp);
	if (ret != 0)
		error(0, errno, "TB: exec failed");
}

/** Fork parent process, daemonize child and execute.
 * \param argv argument vector
 */
void PROC_forkAndExec(char * argv[])
{
	if (!PROC_fork())
		return; /* parent */
	/* daemonize and chdir to root */
	if (daemon(0, 1) < 0)
		error(0, errno, "PROC: daemon failed");
	else
		PROC_execAndFinalize(argv);
}

/** Execute a command and return.
 * \note If the parent process should not wait, you should PROC_fork() first
 *       and either exit or use PROC_execAndFinalize as last call.
 * \param argv argument vector.
 * \return true if call succeeded
 */
bool PROC_execAndReturn(char * argv[])
{
	pid_t pid = fork();
	if (!pid)
		PROC_execAndFinalize(argv);

	int status;
	waitpid(pid, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}
