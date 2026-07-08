#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <termios.h>
#include <ctype.h>
#include "utils.h"
#include "termctl.h"
#include "d_array.h"
#include "ansi.h"

#define PULL_TIMEOUT_MS (500)

/**
 * @brief Internal structure for termctl.
 * @details This structure holds the state and configuration for the terminal control.
 */
typedef struct _termctl_internal_t {
	/**< Array of file descriptors to poll */
	struct pollfd* fds;
	/**< Number of file descriptors */
	size_t nfds;
	/**< Poll timeout in milliseconds */
	size_t poll_timeout_ms;
	/**< Flag indicating if stdin has reached EOF */
	int stdin_eof;
	/**< Prompt callback function */
	termctl_prompt_callback_t prompt_callback;
	/**< Newline callback function */
	termctl_newline_callback_t newline_callback;
	/**< User input callback function */
	termctl_user_input_callback_t user_input_callback;
	/**< Reconnect callback function */
	termctl_reconnect_callback_t reconnect_callback;
	/**< Readline_ex context */
	rlx_t rlx;
	/**< Current prompt string */
	char* prompt;
	/**< Flag indicating if the cursor is at a new line */
	bool atNewLine;
	/**< User data pointer */
	void* context;
} termctl_internal_t;

/**
 * @brief Callback function for readline_ex.
 *
 * @param h The readline_ex handle.
 * @param line The input line.
 * @param length The length of the input line.
 * @param context User data pointer.
 * @details This function is called by readline_ex when user input is received.
 * It processes commands and invokes the user input callback if set.
 * The sole purpose of this function is to intercept STDIN EOF and notify the termctl instance,
 * otherwise we could have done without it.
 */
static void termctl_rlx_callback(rlx_t h, const char* line, size_t length, void* context) {
	(void)h;
	termctl_internal_t* tc = (termctl_internal_t*)context;
	ASSERT(tc);
	if( ! line ) {
		tc->stdin_eof = 1; // tell poll we're done reading from stdin (EOF)
		tc->fds[0].fd = -1; // mark stdin as closed for polling
	} else if( ! rlx_process_command(tc->rlx, line) ) {
		// command not handled by readline_ex, pass it to the user input callback if set
		if( tc->user_input_callback ) {
			tc->user_input_callback((termctl_t)tc, line, length, tc->context);
		}
	}
}

/**
 * @brief Create a new termctl instance.
 *
 * @param appname Name of the application.
 * @param context User data pointer.
 * @return termctl_t A new termctl instance, or NULL on failure.
 */
termctl_t termctl_create(const char* appname, void* context) {
	termctl_internal_t* tc = (termctl_internal_t*)MALLOC(sizeof(termctl_internal_t));
	if( ! tc ) {
		return NULL;
	}

	tc->fds = NULL;
	tc->nfds = 0;
	tc->poll_timeout_ms = PULL_TIMEOUT_MS; // default timeout
	tc->stdin_eof = 0;
	tc->prompt_callback = NULL;
	tc->newline_callback = NULL;
	tc->user_input_callback = NULL;
	tc->prompt = NULL;
	tc->atNewLine = true;
	tc->context = context;
	tc->rlx = rlx_begin(appname, NULL, termctl_rlx_callback, 200, NULL,
		RLX_OPT_PERSIST_HISTORY | RLX_OPT_AUTOCOMPLETE_COMMANDS | RLX_OPT_AUTOCOMPLETE_FILES, tc);

	if( ! tc->rlx ) {
		DEBUG_MSG("Failed to initialize readline_ex");
		FREE(tc);
		return NULL;
	}

	// add stdin for user input
	termctl_add_fd(tc, fileno(stdin));

	return tc;
}

/**
 * @brief Destroy a termctl instance.
 *
 * @param termctl The termctl instance to destroy.
 */
void termctl_destroy(termctl_t termctl) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	if( tc->rlx ) {
		rlx_end(tc->rlx);
	}
	if( tc->fds ) {
		FREE(tc->fds);
	}
	if( tc->prompt ) {
		FREE(tc->prompt);
	}
	FREE(tc);
}

/**
 * @brief Set the prompt callback function for a termctl instance.
 *
 * @param termctl The termctl instance.
 * @param callback The prompt callback function.
 */
void termctl_set_prompt_callback(termctl_t termctl, termctl_prompt_callback_t callback) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	tc->prompt_callback = callback;
}

/**
 * @brief Set the newline callback function for a termctl instance.
 *
 * @param termctl The termctl instance.
 * @param callback The newline callback function.
 */
void termctl_set_newline_callback(termctl_t termctl, termctl_newline_callback_t callback) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	tc->newline_callback = callback;
}

/**
 * @brief Set the user input callback function for a termctl instance.
 *
 * @param termctl The termctl instance.
 * @param callback The user input callback function.
 */
void termctl_set_user_input_callback(termctl_t termctl, termctl_user_input_callback_t callback) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	tc->user_input_callback = callback;
}

/**
 * @brief Set the reconnect callback function for a termctl instance.
 *
 * @param termctl The termctl instance.
 * @param callback The reconnect callback function.
 */
void termctl_set_reconnect_callback(termctl_t termctl, termctl_reconnect_callback_t callback) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	tc->reconnect_callback = callback;
}

/**
 * @brief Get the rlx handle from a termctl instance.
 *
 * @param termctl The termctl instance.
 * @return rlx_t The rlx handle.
 * @details This function allows access to the underlying readline_ex context.
 */
rlx_t termctl_get_rlx(termctl_t termctl) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	return tc->rlx;
}

/**
 * @brief Add a file descriptor to the termctl instance for polling.
 *
 * @param termctl The termctl instance.
 * @param fd The file descriptor to add.
 * @param events The events to monitor.
 * @return int 1 on success, 0 on failure.
 */
int termctl_add_fd(termctl_t termctl, int fd) {
	ASSERT(termctl);
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	// Check if the fd is already added
	for(size_t i=0; i < tc->nfds; i++) {
		if( tc->fds[i].fd == fd ) {
			DEBUG_MSG("File descriptor %d has already been added", fd);
			return 0;
		}
	}
	tc->fds = (struct pollfd*)REALLOC(tc->fds, sizeof(struct pollfd) * (tc->nfds + 1));
	if( ! tc->fds ) {
		return 0;
	}
	tc->fds[tc->nfds].fd = fd;
	tc->fds[tc->nfds].events = POLLIN;
	tc->nfds++;
	return 1;
}

/**
 * @brief Remove a file descriptor from the termctl instance.
 *
 * @param termctl The termctl instance.
 * @param fd The file descriptor to remove.
 * @return int 1 on success, 0 on failure.
 */
int termctl_remove_fd(termctl_t termctl, int fd) {
	ASSERT(termctl);
	ASSERT(fd != fileno(stdin)); // cannot remove stdin from the termctl instance
	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	if( fd == fileno(stdin) ) return 0;
	for(size_t i=0; i < tc->nfds; i++) {
		if( tc->fds[i].fd != fd ) continue;
		// remove this fd by shifting the rest of the array
		for(size_t j=i; j < tc->nfds - 1; j++) {
			tc->fds[j] = tc->fds[j + 1];
		}
		tc->nfds--;
		tc->fds = (struct pollfd*)REALLOC(tc->fds, sizeof(struct pollfd) * tc->nfds);
		return 1;
	}
	return 0; // fd not found
}

/**
 * @brief Update the prompt for a termctl instance.
 *
 * @param tc The termctl internal instance.
 */
static void termctl_update_prompt(termctl_internal_t* tc) {
	ASSERT(tc);
	if( ! tc->prompt_callback ) {
		return;
	}
	if( tc->prompt ) {
		FREE(tc->prompt);
		tc->prompt = NULL;
	}
	char* newPrompt = tc->prompt_callback(tc, tc->context);
	if( newPrompt ) {
		// create a safe version of the new prompt
		char* pSafe = NULL;
		rlx_make_safe_prompt(newPrompt, &pSafe);
		FREE(newPrompt);
		tc->prompt = pSafe;
	}
	rlx_change_prompt(tc->rlx, tc->prompt);
}

/**
	@brief Inject input into the termctl session.
	@param termctl The termctl session handle.
	@param input The input string to inject.
*/
void termctl_inject_input(termctl_t termctl, const char* input) {
	ASSERT(termctl);
	rlx_inject_input(termctl_get_rlx(termctl), input);
}

/**
 * @brief Run a script file.
 * @param tc The termctl instance.
 * @param scriptPath The path to the script file.
 * @return bool True if the script was run successfully, false otherwise.
 */
bool run_script(termctl_t tc, const char* scriptPath, bool ignoreErrors) {
	ASSERT(tc);
	ASSERT(scriptPath);
	FILE* scriptFile = fopen(scriptPath, "r");
	if( ! scriptFile ) {
		if( ! ignoreErrors ) {
			a_error("Failed to open script file: %s\n", scriptPath);
		}
		return false;
	}

	char *line = NULL;
	size_t linecap = 0;
	for(;;) {
		if( getline(&line, &linecap, scriptFile) == -1 ) break;
		// remove trailing newline
		char* p = line;
		while( isspace((unsigned char)*p) ) p++;
		if( ! *p || *p == '#' ) continue; // skip empty lines and comments
		// inject the line into the input buffer of termctl
		termctl_inject_input(tc, p);
	}
	fclose(scriptFile);
	if( line ) {
		free(line);
	}
	return true;
}

/**
 * @brief Run the event loop for a termctl instance.
 *
 * @param termctl The termctl instance.
 * @return termctl_result_t The result of the event loop.
 */
termctl_result_t termctl_event_loop(termctl_t termctl) {
	ASSERT(termctl);

	termctl_internal_t* tc = (termctl_internal_t*)termctl;
	char buffer[128];
	termctl_result_t rc = TERMCTL_R_OK;
	d_array_t retrySet = d_array_create(0, NULL);

	rlx_rebuild_completion_vocabulary(termctl_get_rlx(termctl), NULL);

	while( rc == TERMCTL_R_OK ) {
		termctl_update_prompt(tc);

		int ret = poll(tc->fds, tc->nfds, tc->poll_timeout_ms);

		// Check for poll errors
		if( ret < 0 ) {
			DEBUG_MSG("Error during poll: %s", strerror(errno));
			rc = TERMCTL_R_POLLERROR;
			break;
		}

		// handle poll timeout.
		// we take this opportunity to run periodic checkups
		if( ret == 0 ) {
			// if STDIN was closed, close this termctl session.
			// (tc->stdin_eof is set in termctl_rlx_callback() when it detects EOF on stdin)
			if( tc->stdin_eof ) {
				// end session
				rc = TERMCTL_R_OK;
				break;
			}
			// attempt to reconnect any disconnected fds
			for(size_t i=0; i < d_array_size(retrySet); i++) {
				int fd = (int)(intptr_t)d_array_get(retrySet, i);
				ASSERT(fd > 0);
				int newFd = tc->reconnect_callback(tc, fd, tc->context);
				if( newFd > 0 ) {
					termctl_add_fd(tc, newFd);
					// remove the fd from the retry set
					d_array_remove(retrySet, i);
					--i; // adjust index since we removed an element
				}
			}
			continue; // continue polling
		}

		// Check if user input is available
		if( tc->fds[0].revents & POLLIN ) {
			// trigger readline_ex to read the input and call our RLX callback function
			rlx_process_input(tc->rlx);
		} // end stdin handling

		// check for events on other file descriptors
		for(size_t i=1; i < tc->nfds; i++) {
			if( ! (tc->fds[i].revents & POLLIN) ) continue; // no events from this fd
			ssize_t bytesRead = read(tc->fds[i].fd, buffer, sizeof(buffer) - 1);
			if( bytesRead < 0 ) {
				// error reading from this fd:
				// remove it from the poll list and add it to the retry-set
				// (if a reconnect callback is set)
				if( tc->reconnect_callback ) {
					d_array_add(retrySet, (void*)(intptr_t)tc->fds[i].fd);
				}
				close(tc->fds[i].fd);
				termctl_remove_fd(tc, tc->fds[i].fd);
			} else {
				rlx_pause(tc->rlx);
				buffer[bytesRead] = '\0'; // null-terminate the buffer
				for(const char *p = buffer; *p; ++p) {
					if( *p == '\n' ) {
						fputc(*p, stdout);
						tc->atNewLine = true;
					} else if( *p == '\r' ) {
						// ignore carriage return, as we handle newlines with '\n'
					} else {
						if( tc->atNewLine && tc->newline_callback ) {
							tc->newline_callback(tc, tc->context);
						}
						fputc(*p, stdout);
						tc->atNewLine = false;
					}
				}
				fflush(stdout);
				rlx_resume(tc->rlx, tc->atNewLine);
			}
		} // end other fds handling
	} // end while

	d_array_destroy(retrySet);

	return rc;
}
