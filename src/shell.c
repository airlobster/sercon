#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <sys/errno.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "command.h"
#include "r_buffer.h"
#include "shell.h"

#define POLL_TIMEOUT (1000) // milliseconds
#define POLL_MASK (POLLIN | POLLHUP | POLLERR)

static void close_pipe(int pipefd[2]) {
	for(int i = 0; i < 2; ++i) {
		if( pipefd[i] != -1 ) {
			close(pipefd[i]);
			pipefd[i] = -1;
		}
	}
}

/**
 * @brief Execute a shell command.
 * @param argv The argument vector for the command. It is assumed that argv's last element is NULL.
 * @param input Optional input to pass to the command's stdin.
 * @param stdout_callback Callback function for handling stdout output.
 * @param stderr_callback Callback function for handling stderr output.
 * @param context User-defined data to pass to the callback functions.
 * @return int The exit status of the command, or -1 if sc_shell itself fails.
 */
int sc_shell_v(
		const char* argv[],
		const char* input,
		shell_output_callback_t stdout_callback,
		shell_output_callback_t stderr_callback,
		void* context
	) {
	int pStdin[2]={-1,-1}, pStdout[2]={-1,-1}, pStderr[2]={-1,-1};
	char buffer[64];
	int ret = 0;
	int childExitCode = -1;

	ASSERT(argv && argv[0]);
	if( ! argv || ! argv[0] ) {
		return -1;
	}

	// create pipes for stdin, stdout, and stderr
	if( pipe(pStdin) != 0 || pipe(pStdout) != 0 || pipe(pStderr) != 0 ) {
		perror("pipe failed");
		close_pipe(pStdin);
		close_pipe(pStdout);
		close_pipe(pStderr);
		return -1;
	}

	pid_t pid = fork();
	if( pid == 0 ) {
		// CHILD PROCESS
		close(pStdin[1]); // Close write end of stdin pipe
		close(pStdout[0]); // Close read end of stdout pipe
		close(pStderr[0]); // Close read end of stderr pipe

		dup2(pStdin[0], STDIN_FILENO);
		dup2(pStdout[1], STDOUT_FILENO);
		dup2(pStderr[1], STDERR_FILENO);

		close(pStdin[0]); // Close original read end of stdin pipe
		close(pStdout[1]); // Close original write end of stdout pipe
		close(pStderr[1]); // Close original write end of stderr pipe

		execvp(argv[0], (char * const *)argv);

		perror("execvp failed");

		exit(1);
	} // end child process

	// PARENT PROCESS
	close(pStdin[0]); // Close read end of stdin pipe
	close(pStdout[1]); // Close write end of stdout pipe
	close(pStderr[1]); // Close write end of stderr pipe

	// Write input to child's stdin if provided
	if( input ) {
		write(pStdin[1], input, strlen(input));
	}
	close(pStdin[1]);

	struct pollfd fds[] = {
		{ .fd = pStdout[0], .events = POLL_MASK },
		{ .fd = pStderr[0], .events = POLL_MASK }
	};

	int lastChar = 0;
	int fdsLeft = array_size(fds);
	while( fdsLeft ) {
		ret = poll(fds, array_size(fds), POLL_TIMEOUT);

		if( ret < 0 ) {
			perror("poll failed");
			break;
		}

		// read child's STDOUT
		if( fds[0].revents & POLL_MASK ) {
			ssize_t bytesRead = read(pStdout[0], buffer, sizeof(buffer) - 1);
			if( bytesRead > 0 ) {
				buffer[bytesRead] = '\0';
				lastChar = buffer[bytesRead - 1];
				if( stdout_callback ) {
					stdout_callback(buffer, bytesRead, context);
				}
			} else {
				fds[0].fd = -1; // Mark this fd as closed
				--fdsLeft;
			}
		}

		// read child's STDERR
		if( fds[1].revents & POLL_MASK ) {
			ssize_t bytesRead = read(pStderr[0], buffer, sizeof(buffer) - 1);
			if( bytesRead > 0 ) {
				buffer[bytesRead] = '\0';
				lastChar = buffer[bytesRead - 1];
				if( stderr_callback ) {
					stderr_callback(buffer, bytesRead, context);
				}
			} else {
				fds[1].fd = -1; // Mark this fd as closed
				--fdsLeft;
			}
		}
	} // end poll loop

	if( lastChar != '\n' && stdout_callback ) {
		stdout_callback("\n", 1, context);
	}

	close(pStdout[0]);
	close(pStderr[0]);

	int childExitStatus = 0;
	if( waitpid(pid, &childExitStatus, 0) == -1 ) {
		// DEBUG_MSG("waitpid failed: %s", strerror(errno));
	} else if( WIFEXITED(childExitStatus) ) {
		childExitCode = WEXITSTATUS(childExitStatus);
		// DEBUG_MSG("Child exited with code %d", childExitCode);
	} else if( WIFSIGNALED(childExitStatus) ) {
		// DEBUG_MSG("Child terminated by signal %d", WTERMSIG(childExitStatus));
	}

	return childExitCode;
}

/**
 * @brief Execute a shell command given as a single string.
 * @param command The command string to execute.
 * @param input Optional input to pass to the command's stdin.
 * @param stdout_callback Callback function for handling stdout output.
 * @param stderr_callback Callback function for handling stderr output.
 * @param context User-defined data to pass to the callback functions.
 * @return int The exit status of the command, or -1 if sc_shell itself fails.
 */
int sc_shell(
	const char* command,
	const char* input,
	shell_output_callback_t stdout_callback,
	shell_output_callback_t stderr_callback,
	void* context
) {
	int argc = 0;
	char** argv = 0;
	ASSERT(command);
	if( ! command ) return -1;
	if( ! parse_command_line(command, &argc, &argv) ) {
		DEBUG_MSG("Failed to parse command: %s", command);
		free_command_args(argc, argv);
		return -1;
	}
	int ret = sc_shell_v((const char**)argv, input, stdout_callback, stderr_callback, context);
	free_command_args(argc, argv);
	return ret;
}
