#ifndef _READLINE_EX_H
#define _READLINE_EX_H

/**
	@file readline_ex.h
	@brief readline_ex is a wrapper around the GNU readline library that provides some additional features.
	@details Features include:
	- support for registering commands with associated handlers and descriptions,
		and processing user input to execute those commands.
	- support for autocompletion.
	- default autocompletion vocabulary that can include both registered commands
		and command history entries, with options to configure this.
	- ability to set a custom autocompletion vocabulary.
	- support for pausing and resuming readline to allow printing output from other
		file-descriptors without interfering with the user's current input.
	- support for persisting command history to a file and loading it on startup,
		with optional context-based history files.
	- full shell-like history management support.

	(GNU readline Library: https://tiswww.case.edu/php/chet/readline/rltop.html)
	(GNU history library: https://tiswww.case.edu/php/chet/readline/history.html)
*/

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* rlx_t;

typedef struct _rlx_registered_command_t {
	int id;
	const char* command;
	const char* description;
	void(*handler)(const struct _rlx_registered_command_t* cmd, int argc, const char *argv[], void* userData);
} rlx_registered_command_t;

typedef enum {
	RLX_OPT_AUTOCOMPLETE_HISTORY = 1U << 0, // include history entries in the autocomplete vocabulary
	RLX_OPT_AUTOCOMPLETE_COMMANDS = 1U << 1, // include registered commands in the autocomplete vocabulary
	RLX_OPT_AUTOCOMPLETE_FILES = 1U << 2, // default GNU readline file autocompletion (ignoring any custom vocabulary)
	RLX_OPT_AUTOCOMPLETE_MASK = RLX_OPT_AUTOCOMPLETE_HISTORY | RLX_OPT_AUTOCOMPLETE_COMMANDS | RLX_OPT_AUTOCOMPLETE_FILES,
	RLX_OPT_PERSIST_HISTORY = 1U << 3, // save history to file and load it on startup
} rlx_options_t;

// RLX session management
rlx_t rlx_begin(
	const char* appname,
	const char* prompt,
	void (*callback)(rlx_t h, char*),
	size_t maxHistoryEntries,
	const char* historyContext,
	unsigned long options
);
void rlx_end(rlx_t h);
void rlx_pause(rlx_t h);
void rlx_resume(rlx_t h, bool redisplayPrompt);

// called whenever content is available for reading from stdin,
// typically from an event loop or select/poll/epoll callback
void rlx_process_input(rlx_t h);

// registered commands
void rlx_register_commands(rlx_t h, const rlx_registered_command_t* commands);
const rlx_registered_command_t* rlx_get_command(rlx_t h, const char* command);
void rlx_print_registered_commands(rlx_t h);

// processes a line of input, executing the corresponding command handler if a
// registered command is found, otherwise returns false to indicate that the
// line was not recognized as a command and can be processed as regular input by the caller
bool rlx_process_command(rlx_t h, const char* line, void* userData);

// history management
void rlx_reset_history(rlx_t h);
void rlx_print_history(rlx_t h);

// vocabulary management
void rlx_set_autocomplete_vocabulary(rlx_t h, char** vocab);

#ifdef __cplusplus
}
#endif

#endif // _READLINE_EX_H
