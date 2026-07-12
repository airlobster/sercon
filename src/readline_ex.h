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

	@author Adi Degani
	@copyright Copyright (c) 2026 Adi Degani. All rights reserved.
*/

#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a readline_ex session.
 */
typedef struct _rlx_internal_t* rlx_t;

/**
 * @brief Structure representing a registered command.
 * @details This structure holds information about a command, including its ID, command string,
 * description, and a handler function that is called when the command is executed.
 */
typedef struct _rlx_registered_command_t {
	/** The unique identifier for the command. */
	int id;
	/** The command string. */
	const char* command;
	/** A description of the command. */
	const char* description;
	/** The handler function that is called when the command is executed. */
	void(*handler)(rlx_t h, const struct _rlx_registered_command_t* cmd, int argc, const char *argv[], void* context);
} rlx_registered_command_t;

#define RLX_REGISTERED_COMMANDS_END {0, 0, 0, 0} // end marker for registered commands array

/**
 * @brief Options for configuring a readline_ex session.
 * @details These options can be combined using bitwise OR to enable multiple features.
 */
typedef enum {
	RLX_OPT_AUTOCOMPLETE_DEFAULT = 1UL << 0, // readline's default autocompletion behavior (files only)
	RLX_OPT_AUTOCOMPLETE_COMMANDS = 1UL << 1, // use a custom autocomplete vocabulary instead of the default
	RLX_OPT_AUTOCOMPLETE_FILES = 1UL << 2, // enable filename autocompletion
	RLX_OPT_AUTOCOMPLETE_MASK = RLX_OPT_AUTOCOMPLETE_DEFAULT | RLX_OPT_AUTOCOMPLETE_COMMANDS | RLX_OPT_AUTOCOMPLETE_FILES,
	RLX_OPT_PERSIST_HISTORY = 1UL << 5, // save history to file and load it on startup
	RLX_OPT_HISTORY_ALLOW_DUPLICATES = 1UL << 6, // allow duplicate entries in history
	RLX_OPT_NO_TRIM_LINE = 1UL << 7, // do not trim leading and trailing whitespace from input lines before processing
} rlx_options_t;

/**
 * @brief Callback function type for handling input lines.
 * @param h The readline_ex session handle.
 * @param line The input line.
 * @param length The length of the input line.
 * @param context User data passed to the callback function.
 */
typedef void (*rlx_callback_t)(rlx_t h, const char* line, size_t length, void* context);

/**
 * @brief Callback function type for building the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 * @param text The text to complete.
 * @param context User data passed to the callback function.
 * @details Use this callback to add words to the auto-complete vocabulary using
 * the rlx_add_autocomplete_vocabulary_entry() function.
 * It is allowed to register multiple such callbacks.
 */
typedef void (*rlx_vocabulary_build_callback_t)(rlx_t rlx, const char* text, void* context);

/**
 * @brief Callback function type for idle actions.
 * @param h The readline_ex session handle.
 * @param context User data passed to the callback function.
 */
typedef void (*rlx_idle_action_callback_t)(rlx_t h, void* context);

// RLX session management
rlx_t rlx_begin(
	const char* appname,
	const char* prompt,
	rlx_callback_t callback,
	size_t maxHistoryEntries,
	const char* historyContext,
	unsigned long options,
	void* context
);
void rlx_end(rlx_t h);
void rlx_pause(rlx_t h);
void rlx_resume(rlx_t h, bool redisplayPrompt);
void rlx_change_prompt(rlx_t h, const char* newPrompt);
void rlx_rebuild_completion_vocabulary(rlx_t rlx, const char* text);

void rlx_set_ctrlC(rlx_t h, bool enabled);

// should be called whenever content is available for reading from stdin,
// typically from an event loop or select/poll/epoll callback
void rlx_process_input(rlx_t h);

const char* rlx_get_current_line(rlx_t h);
void rlx_inject_input(rlx_t h, const char* input);

// registered commands
void rlx_register_commands(rlx_t h, const rlx_registered_command_t* commands);
const rlx_registered_command_t* rlx_get_command(rlx_t h, const char* command);
void rlx_print_registered_commands(rlx_t h);

// processes a line of input, executing the corresponding command handler if a
// registered command is found, otherwise returns false to indicate that the
// line was not recognized as a command and can be processed as regular input by the caller.
// typically called from the RLX callback function provided to rlx_begin().
bool rlx_process_command(rlx_t h, const char* line);

// history management
int rlx_get_history_length(rlx_t h);
void rlx_reset_history(rlx_t h);
void rlx_print_history(rlx_t h);

// vocabulary management
bool rlx_add_autocomplete_callback(rlx_t h, rlx_vocabulary_build_callback_t callback);
bool rlx_add_autocomplete_vocabulary_entry(rlx_t h, const char* entry);
#ifdef _DEBUG_
void rlx_print_autocomplete_vocabulary(rlx_t h);
#endif

void rlx_make_safe_prompt(const char* prompt, char** outSafePrompt);

void rlx_set_idle_action_callback(rlx_t h, rlx_idle_action_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif // _READLINE_EX_H
