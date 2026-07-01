#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <string.h>
#include "utils.h"
#include "shell.h"
#include "ansi.h"

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
 * @param argv The argument vector for the command.
 * @param input Optional input to pass to the command's stdin.
 * @return int The exit status of the command.
 */
int sc_shell(const char* argv[], const char* input) {
	int pStdin[2]={-1,-1}, pStdout[2]={-1,-1}, pStderr[2]={-1,-1};
	char buffer[64];
	int ret = 0;

	if( ! argv || ! argv[0] ) {
		a_error("No command provided to shell\n");
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
	} else {
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
					a_normal("%s", buffer);
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
					a_error("%s", buffer);
				} else {
					fds[1].fd = -1; // Mark this fd as closed
					--fdsLeft;
				}
			}
		} // end poll loop

		if( lastChar != '\n' ) {
			fputc('\n', stdout);
		}

		close(pStdout[0]);
		close(pStderr[0]);

		wait(NULL); // Wait for child process to finish
	}

	return ret;
}
