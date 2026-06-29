#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include "utils.h"
#include "termctl.h"

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
	/**< Readline_ex context */
	rlx_t rlx;
	/**< Current prompt string */
	char* prompt;
	/**< Flag indicating if the cursor is at a new line */
	bool atNewLine;
	/**< User data pointer */
	void* user_data;
} termctl_internal_t;

/**
 * @brief Callback function for readline_ex.
 * 
 * @param h The readline_ex handle.
 * @param line The input line.
 * @param length The length of the input line.
 * @param userData User data pointer.
 */
static void rlx_callback(rlx_t h, const char* line, size_t length, void* userData) {
	(void)h;
	termctl_internal_t* tc = (termctl_internal_t*)userData;
	ASSERT(tc);
	if( line && rlx_process_command(tc->rlx, line) ) {
		// command was recognized and processed by readline_ex. no further handling required.
		return;
	}
	if( tc->user_input_callback ) {
		tc->user_input_callback((termctl_t)tc, line, length, tc->user_data);
	}
}

/**
 * @brief Create a new termctl instance.
 * 
 * @param appname Name of the application.
 * @param userData User data pointer.
 * @return termctl_t A new termctl instance, or NULL on failure.
 */
termctl_t termctl_create(const char* appname, void* userData) {
	termctl_internal_t* tc = (termctl_internal_t*)malloc(sizeof(termctl_internal_t));
	if( ! tc ) {
		DEBUG_MSG("Failed to allocate memory for termctl_internal_t");
		return NULL;
	}

	tc->fds = NULL;
	tc->nfds = 0;
	tc->poll_timeout_ms = 500; // default timeout
	tc->stdin_eof = 0;
	tc->prompt_callback = NULL;
	tc->newline_callback = NULL;
	tc->user_input_callback = NULL;
	tc->prompt = NULL;
	tc->atNewLine = true;
	tc->user_data = userData;
	tc->rlx = rlx_begin(appname, NULL, rlx_callback, 200, NULL, RLX_OPT_AUTOCOMPLETE_COMMANDS, tc);

	if( ! tc->rlx ) {
		DEBUG_MSG("Failed to initialize readline_ex");
		free(tc);
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
		free(tc->fds);
	}
	if( tc->prompt ) {
		free(tc->prompt);
	}
	free(tc);
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
	ASSERT(tc->fds);
	// Check if the fd is already added
	for(size_t i=0; i < tc->nfds; i++) {
		if( tc->fds[i].fd == fd ) {
			DEBUG_MSG("File descriptor %d is already added", fd);
			return 0; // this fd was already added - do nothing
		}
	}
	tc->fds = (struct pollfd*)realloc(tc->fds, sizeof(struct pollfd) * (tc->nfds + 1));
	if( ! tc->fds ) {
		DEBUG_MSG("Failed to allocate memory for pollfd array");
		return 0;
	}
	tc->fds[tc->nfds].fd = fd;
	tc->fds[tc->nfds].events = POLLIN | POLLERR | POLLHUP;
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
		tc->fds = (struct pollfd*)realloc(tc->fds, sizeof(struct pollfd) * tc->nfds);
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
	if( ! tc->prompt_callback ) {
		return;
	}
	if( tc->prompt ) {
		free(tc->prompt);
		tc->prompt = NULL;
	}
	char* newPrompt = tc->prompt_callback(tc, tc->user_data);
	if( newPrompt ) {
		// create a safe version of the new prompt
		char* pSafe = NULL;
		rlx_make_safe_prompt(newPrompt, &pSafe);
		free(newPrompt);
		tc->prompt = pSafe;
	}
	rlx_change_prompt(tc->rlx, newPrompt);
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
	for(;;) {
		int ret = poll(tc->fds, tc->nfds, tc->poll_timeout_ms);

		// Check for poll errors
		if( ret < 0 ) {
			DEBUG_MSG("Error during poll: %s", strerror(errno));
			rc = TERMCTL_R_POLLERROR;
			break;
		}

		// timeout?
		if( ret == 0 ) {
			if( tc->stdin_eof ) {
				// end of session
				rc = TERMCTL_R_OK;
				break;
			}
			// update the prompt periodically
			// to reflect the current connection status (connected/disconnected)
			if( tc->prompt_callback ) {
				if( tc->prompt ) {
					free(tc->prompt);
					tc->prompt = NULL;
				}
				termctl_update_prompt(tc);
			}
			continue; // continue polling
		}

		// Check if user input is available
		if( tc->fds[0].revents ) {
			tc->stdin_eof = (tc->fds[0].revents & (POLLHUP | POLLERR)) != 0;
			// trigger readline_ex to read the input and call our RLX callback function
			rlx_process_input(tc->rlx);
		} // end stdin handling

		// check for events on other file descriptors
		rlx_pause(tc->rlx);
		for(size_t i=1; i < tc->nfds; i++) {
			if( ! tc->fds[i].revents ) continue; // no events from this fd
			ssize_t bytesRead = read(tc->fds[i].fd, buffer, sizeof(buffer) - 1);
			if( bytesRead < 0 ) {
				// a_error("Error reading from serial port: %s\n", strerror(errno));
				close(tc->fds[i].fd);
				termctl_remove_fd(tc, tc->fds[i].fd);
				rc = TERMCTL_R_READERROR;
				break;
			}
			buffer[bytesRead] = '\0'; // null-terminate the buffer
			for(const char *p = buffer; *p; ++p) {
				if( *p == '\n' ) {
					fputc(*p, stdout);
					tc->atNewLine = true;
				} else if( *p == '\r' ) {
					// ignore carriage return, as we handle newlines with '\n'
				} else {
					if( tc->atNewLine && tc->newline_callback ) {
						tc->newline_callback(tc, tc->user_data);
					}
					fputc(*p, stdout);
					tc->atNewLine = false;
				}
			}
			fflush(stdout);
		} // end other fds handling
		rlx_resume(tc->rlx, tc->atNewLine);
	} // end while

	return rc;
}
