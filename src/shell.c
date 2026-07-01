#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/poll.h>
#include <string.h>
#include "utils.h"
#include "shell.h"
#include "ansi.h"

/**
 * @brief Execute a shell command.
 * @param argv The argument vector for the command.
 * @param input Optional input to pass to the command's stdin.
 * @return int The exit status of the command.
 */
int shell(const char* argv[], const char* input) {
	int pStdin[2], pStdout[2], pStderr[2];
	char buffer[64];

	if( ! argv || ! argv[0] ) {
		a_error("No command provided to shell\n");
		return -1;
	}

	pipe(pStdin);
	pipe(pStdout);
	pipe(pStderr);

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

		if( input ) {
			write(pStdin[1], input, strlen(input));
		}
		close(pStdin[1]);

		struct pollfd fds[] = {
			{ .fd = pStdout[0], .events = POLLIN },
			{ .fd = pStderr[0], .events = POLLIN }
		};

		int lastChar = 0;
		for(;;) {
			int ret = poll(fds, array_size(fds), -1);
			if( ret < 0 ) {
				perror("poll failed");
				return -1;
			}

			// read child's STDOUT
			if( fds[0].revents & POLLIN ) {
				ssize_t bytesRead = read(pStdout[0], buffer, sizeof(buffer) - 1);
				if( bytesRead > 0 ) {
					buffer[bytesRead] = '\0';
					fprintf(stdout, "%s", buffer);
					lastChar = buffer[bytesRead - 1];
				} else {
					break; // EOF on stdout
				}
			}

			// read child's STDERR
			if( fds[1].revents & POLLIN ) {
				ssize_t bytesRead = read(pStderr[0], buffer, sizeof(buffer) - 1);
				if( bytesRead > 0 ) {
					buffer[bytesRead] = '\0';
					a_error("%s", buffer);
					lastChar = buffer[bytesRead - 1];
				} else {
					break; // EOF on stderr
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

	return 0;
}
