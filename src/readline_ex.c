#include <stdio.h>
#include <pwd.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <assert.h>
#include "readline_ex.h"
#include "command.h"

#define ASSERT(x) assert(x)

// registered command linked list node type
typedef struct _rlx_command_node_t {
	struct _rlx_command_node_t* next;
	rlx_registered_command_t cmd;
} rlx_command_node_t;

typedef struct _rlx_vocabulary_extension_t {
	struct _rlx_vocabulary_extension_t* next;
	char *entry;
} rlx_vocabulary_extension_t;

// internal readline context structure type
typedef struct {
	bool isInitialized;
	rlx_callback_t callback;
	void* userData;
	unsigned long options;
	const char* prompt;
	char* historyFilePath;
	size_t maxHistoryEntries;
	char* savedLineBuffer;
	rlx_command_node_t* commands;
	char** completionVocabulary;
	bool ownsCompletionVocabulary;
	rlx_vocabulary_extension_t* vocabularyExtensions;
	bool isPaused;
} rlx_internal_t;

// singleton readline context
static rlx_internal_t rlxStatic = {0};

// forward local declarations
static char** rlx_build_completion_vocabulary(rlx_t h);
static void rlx_free_completion_vocabulary(char** vocab);
static char** rlx_custom_completion(const char* text, int start, int end);
static char* rlx_custom_completion_generator(const char* text, int state);
static void rlx_add_history_entry(rlx_t h, const char* line);
static void rlx_commit_history(rlx_t h);

/**
	@brief Find the start and end of the non-whitespace content in a string.
	@param s The input string.
	@param start Pointer to store the start of the content.
	@param end Pointer to store the end of the content.
	@return The length of the content.
*/
static int strnetcontent(char* s, char** start, char** end) {
	ASSERT(start && end);
	*start = *end = 0;
	if( ! s ) return 0;
	register char *p = s;
	// skip over leading whitespace
	while( isspace(*p) ) {
		++p;
	}
	*start = *end = p;
	// find the last non-whitespace character
	while( *p ) {
		if( ! isspace(*p) ) {
			*end = p + 1;
		}
		++p;
	}
	ASSERT(*end >= *start);
	return *end - *start;
}

/**
	@brief Create the file path for the history file.
	@param appname The name of the application.
	@param historyContext The context for the history file (optional).
	@return The file path for the history file.
*/
static char* makeHistoryFilePath(const char* appname, const char* historyContext) {
	char* path = 0;
	const char* homeDir = getenv("HOME");
	if( ! homeDir ) {
		homeDir = getpwuid(getuid())->pw_dir;
	}
	if( historyContext && *historyContext ) {
		// if a history context is provided, we will include it in the history file name
		// to allow separate histories for different contexts (e.g. different serial ports).
		char* historyContextCpy = strdup(historyContext);
		// normalize the history context by replacing any non-alphanumeric characters with underscores
		for(char* p = historyContextCpy; *p; p++) {
			if( ! isalnum(*p) ) {
				*p = '_'; // replace any non-alphanumeric characters with underscores to ensure a valid file name
			}
		}
		asprintf(&path, "%s/.%s.%s.rlx.history", homeDir, appname, historyContextCpy);
		free(historyContextCpy);
	} else {
		asprintf(&path, "%s/.%s.rlx.history", homeDir, appname);
	}
	return path;
}

/**
 * @brief Wrapper for the readline callback for boilerplate handling.
 * @details This function is called by readline when the user inputs a line.
 * ## Cleans up the input line from leading and trailing whitespace
 * ## checks if it's not empty, and then calls the user-provided callback with the cleaned line content.
 * ## calls the user-provided callback with the cleaned line content.
 * ## Free the line buffer
 * ## notifying the callback about EOF conditions.
 * @param line The input line from readline.
 */
static void readline_callback_wrapper(char* line) {
	rlx_internal_t* rlx = &rlxStatic;
	ASSERT(rlx->isInitialized);
	ASSERT(rlx->callback);
	if( line ) {
		if( ! (rlx->options & RLX_OPT_NO_TRIM_LINE) ) {
			char *start, *end;
			if( strnetcontent(line, &start, &end) ) {
				*end = '\0'; // null-terminate the line at the end of the net content
				rlx->callback((rlx_t)rlx, start, end - start, rlx->userData);
			} else {
				// ignore empty lines (only whitespace)
			}
		} else {
			rlx->callback((rlx_t)rlx, line, strlen(line), rlx->userData);
		}
		free(line); // free the line buffer after processing to avoid memory leaks
	} else {
		// EOF received (e.g., Ctrl+D). notify the callback with a NULL line
		rlx->callback((rlx_t)rlx, 0, 0, rlx->userData);
	}
}

/**
	@brief Begin a readline_ex session.
	@param appname The name of the application.
	@param prompt The prompt string to display.
	@param callback The callback function to handle input lines.
	@param maxHistoryEntries The maximum number of history entries to keep.
	@param historyContext The context for the history file (optional).
	@param options Options for configuring the readline_ex session.
	@param userData User data to pass to the callback function and registered command handlers.
	@return A handle to the readline_ex session, or NULL on failure.
*/
rlx_t rlx_begin(
	const char* appname,
	const char* prompt,
	rlx_callback_t callback,
	size_t maxHistoryEntries,
	const char* historyContext,
	unsigned long options,
	void* userData
) {
	rlx_internal_t* rlx = &rlxStatic;

	ASSERT(appname && *appname);
	ASSERT(callback);

	// allow only a single readline context since readline uses global state !!!
	if( rlx->isInitialized ) return 0;

	rlx->isInitialized = true;
	rlx->callback = callback;
	rlx->userData = userData;
	rlx->options = options;
	rlx->prompt = prompt;
	rlx->historyFilePath = makeHistoryFilePath(appname, historyContext);
	rlx->maxHistoryEntries = MAX(maxHistoryEntries, 10);
	rlx->savedLineBuffer = 0;
	rlx->commands = 0;
	rlx->completionVocabulary = 0;
	rlx->ownsCompletionVocabulary = true;
	rlx->vocabularyExtensions = 0;
	rlx->isPaused = false;

	// if the option to persist history is enabled, we will load the history from the file on startup
	if( rlx->options & RLX_OPT_PERSIST_HISTORY ) {
		read_history(rlx->historyFilePath);
	}

	rl_callback_handler_install(rlx->prompt, readline_callback_wrapper);

	// setup auto-complete
	// (RLX_OPT_AUTOCOMPLETE_COMMANDS and RLX_OPT_AUTOCOMPLETE_HISTORY takes priority
	// over RLX_OPT_AUTOCOMPLETE_FILES)
	if( rlx->options & RLX_OPT_AUTOCOMPLETE_MASK ) {
		// some kind of auto-completion was requested,
		// so we set up the completion function and word break characters accordingly.
		if( rlx->options & (RLX_OPT_AUTOCOMPLETE_COMMANDS | RLX_OPT_AUTOCOMPLETE_HISTORY) ) {
			rl_attempted_completion_function = rlx_custom_completion;
		} else if( rlx->options & RLX_OPT_AUTOCOMPLETE_FILES ) {
			rl_attempted_completion_function = 0; // use default readline filename completion
		}
		rl_bind_key('\t', rl_complete);
		rl_completer_word_break_characters = " \t\n\"'`@$><=;|&{(";
	} else {
		// auto-complete is disabled
		rl_bind_key('\t', rl_insert);
	}

	return (rlx_t)rlx;
}

/**
	@brief End the readline_ex session, cleaning up resources.
	@param h The readline_ex session handle.
*/
void rlx_end(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;

	ASSERT(rlx);
	ASSERT(rlx->isInitialized);

	if( ! rlx || ! rlx->isInitialized ) return;

	rl_callback_handler_remove();

	rlx_commit_history(h);

	free(rlx->historyFilePath);
	rlx->historyFilePath = 0;

	if( rlx->savedLineBuffer ) {
		free(rlx->savedLineBuffer);
		rlx->savedLineBuffer = 0;
	}

	// free the registered commands linked list
	for(rlx_command_node_t* cmd = rlx->commands; cmd; ) {
		rlx_command_node_t* next = cmd->next;
		free(cmd);
		cmd = next;
	}

	// free auto-complete vocabulary
	if( rlx->ownsCompletionVocabulary ) {
		rlx_free_completion_vocabulary(rlx->completionVocabulary);
		rlx->completionVocabulary = 0;
	}

	for(rlx_vocabulary_extension_t* ext = rlx->vocabularyExtensions; ext; ) {
		rlx_vocabulary_extension_t* next = ext->next;
		free(ext->entry);
		free(ext);
		ext = next;
	}
	rlx->vocabularyExtensions = 0;

	// reset the readline context to its initial state
#ifdef NDEBUG
	memset(rlx, 0, sizeof(*rlx));
#else
	memset(rlx, -1, sizeof(*rlx));
#endif
	rlx->isInitialized = false;
}

/**
 * @brief Change the prompt string for the readline_ex session.
 * @param h The readline_ex session handle.
 * @param newPrompt The new prompt string to display.
 * @note: previous prompt string will not be freed because it is owned and managed
 * by the application itself.
 */
void rlx_change_prompt(rlx_t h, const char* newPrompt) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	ASSERT(newPrompt);
	rlx->prompt = newPrompt;
	fputc('\r', stdout); // move cursor to the beginning of the line
	rl_replace_line("", 0); // clear any partial input from the user while waiting for serial input
	rl_callback_handler_install(rlx->prompt, readline_callback_wrapper);
}

/**
	@brief Register commands with the readline_ex session.
	@param h The readline_ex session handle.
	@param commands An array of commands to register.
*/
void rlx_register_commands(rlx_t h, const rlx_registered_command_t* commands) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	ASSERT(commands);
	for( const rlx_registered_command_t* rc = commands; rc && rc->command && rc->handler; rc++ ) {
		if( rlx_get_command(h, rc->command) ) continue;
		rlx_command_node_t* cmd = malloc(sizeof(rlx_command_node_t));
		cmd->cmd = *rc;
		cmd->next = rlx->commands;
		rlx->commands = cmd;
	}
}

/**
	@brief Get a registered command by its name.
	@param h The readline_ex session handle.
	@param command The name of the command to retrieve.
	@return A pointer to the registered command, or NULL if not found.
*/
const rlx_registered_command_t* rlx_get_command(rlx_t h, const char* command) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	ASSERT(command && *command);
	for(const rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
		if( strcmp(cmd->cmd.command, command) == 0 ) {
			return &cmd->cmd;
		}
	}
	return 0;
}

/**
	@brief Process a command line, executing the corresponding command handler if a registered command is found.
	@param h The readline_ex session handle.
	@param line The command line to process.
	@return true if a registered command was found and executed, false otherwise.
*/
bool rlx_process_command(rlx_t h, const char* line) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	char* expanded = 0;
	int argc=0;
	char** argv=0;
	const rlx_registered_command_t* cmd = 0;

	ASSERT(rlx);
	if( ! line || ! *line ) return false;

	int expansionResult = history_expand(line, &expanded);
	if( expansionResult < 0 || expansionResult == 2 ) {
		// history expansion failed, or should be ignored
		free(expanded);
		return false;
	} else if( expansionResult > 0 ) {
		line = expanded;
	}

	// we convert the command line into argc/argv format for easier parsing by command handlers,
	// and then free the argv array after processing.
	if( parse_command_line(line, &argc, &argv) > 0 ) {
		if( (cmd = rlx_get_command(h, argv[0])) != 0 ) {
			cmd->handler(h, cmd, argc, (const char**)argv, rlx->userData);
		}
		free_command_args(argc, argv);
	}

	if( expanded ) {
		free(expanded);
	}

	rlx_add_history_entry((rlx_t)rlx, line);

	return cmd != 0;
}

/**
	@brief Pause the readline_ex session, saving the current input line and prompt.
	@param h The readline_ex session handle.
*/
void rlx_pause(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	if( rlx->isPaused ) return; // if we're already paused, no need to do anything
	rlx->savedLineBuffer = strdup(rl_line_buffer); // save the current readline input so we can restore it after printing serial output
	rl_save_prompt(); // save the current prompt in case it gets overwritten by serial output
	rl_replace_line("", 0); // clear any partial input from the user while waiting for serial input
	rl_redisplay();
	rlx->isPaused = true;
}

/**
	@brief Resume the readline_ex session, restoring the saved input line and prompt.
	@param h The readline_ex session handle.
	@param redisplayPrompt Whether to redisplay the prompt and input line.
*/
void rlx_resume(rlx_t h, bool redisplayPrompt) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	if( ! rlx->isPaused ) return; // if we're not currently paused, no need to do anything
	rl_restore_prompt(); // restore the prompt that was saved before we paused readline for printing serial output
	rl_replace_line(rlx->savedLineBuffer, 0); // restore the saved input line so the user can continue editing it after we print serial output
	if( redisplayPrompt ) {
		rl_redisplay(); // redraw the prompt and any user input after printing serial output
	}
	free(rlx->savedLineBuffer);
	rlx->savedLineBuffer = 0;
	rlx->isPaused = false;
}

/**
	@brief Add a line to the command history for the readline_ex session.
	@param h The readline_ex session handle.
	@param line The line to add to the history.
*/
static void rlx_add_history_entry(rlx_t h, const char* line) {
	(void)h;
	ASSERT(h);
	if( ! line || ! *line ) return;
	add_history(line);
}

/**
	@brief Process input for the readline_ex session.
	@param h The readline_ex session handle.
*/
void rlx_process_input(rlx_t h) {
	(void)h;
	ASSERT(h);
	// this will trigger readline to read the input and call our readline_callback function
	rl_callback_read_char();
}

/**
	@brief Reset the command history for the readline_ex session.
	@param h The readline_ex session handle.
*/
void rlx_reset_history(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	(void)rlx;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	rl_clear_history();
}

/**
 * @brief Get the number of entries in the command history for the readline_ex session.
 * @param h The readline_ex session handle.
 * @return int The number of entries in the command history.
 */
int rlx_get_history_length(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	if( ! rlx->isInitialized ) return 0;
	using_history();
	HISTORY_STATE* historyState = history_get_history_state();
	int length = historyState ? historyState->length : 0;
	free(historyState);
	return length;
}

/**
	@brief Commit the command history for the readline_ex session.
	@param h The readline_ex session handle.
*/
static void rlx_commit_history(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	if( rlx->options & RLX_OPT_PERSIST_HISTORY ) {
		write_history(rlx->historyFilePath);
		history_truncate_file(rlx->historyFilePath, rlx->maxHistoryEntries);
	}
}

/**
	@brief Print the command history for the readline_ex session.
	@param h The readline_ex session handle.
*/
void rlx_print_history(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	(void)rlx;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	int n = rlx_get_history_length(h);
	if( n <= 0 ) return;
	using_history();
	for(int i=0; i < n; i++) {
		HIST_ENTRY* entry = history_get(history_base + i);
		if( ! entry || ! entry->line ) continue;
		printf("%d: %s\n", history_base + i, entry->line);
	}
}

static int compare_commands(const void* a, const void* b) {
	const rlx_registered_command_t* cmdA = *(const rlx_registered_command_t**)a;
	const rlx_registered_command_t* cmdB = *(const rlx_registered_command_t**)b;
	return strcasecmp(cmdA->command, cmdB->command);
}
/**
	@brief Print the registered commands for the readline_ex session.
	@param h The readline_ex session handle.
*/
void rlx_print_registered_commands(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	int i = 0;

	ASSERT(rlx);
	if( ! rlx->commands || ! rlx->commands->cmd.command ) return;

	// count the number of registered commands
	int nCommands = 0;
	for( rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
		++nCommands;
	}

	// create an array of pointers to the registered commands for sorting
	const rlx_registered_command_t** commands = malloc(sizeof(rlx_registered_command_t*) * nCommands);
	for( rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
		commands[i++] = &cmd->cmd;
	}

	// sort the array of command pointers alphabetically by command name
	qsort(commands, nCommands, sizeof(rlx_registered_command_t*), compare_commands);

	// print sorted array of commands with their descriptions
	for(i=0; i < nCommands; i++) {
		const rlx_registered_command_t* cmd = commands[i];
		printf("  %-10s - %s\n",
			cmd->command,
			cmd->description ? cmd->description : "(no description)");
	}

	// cleanup
	free(commands);
}

/**
	@brief Build the completion vocabulary for the readline_ex session.
	@param h The readline_ex session handle.
	@return An array of strings representing the completion vocabulary, or NULL on failure.
*/
static char** rlx_build_completion_vocabulary(rlx_t h) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);

	char** vocab = 0;
	size_t vocabSize = 0;

	if( ! rlx ) return 0;

	if( rlx->options & RLX_OPT_AUTOCOMPLETE_COMMANDS ) {
		// count the number of registered commands
		for( rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
			vocabSize++;
		}
	}
	if( rlx->options & RLX_OPT_AUTOCOMPLETE_HISTORY ) {
		// add the count of history entries to the vocabulary size
		// since we also want to include those in the completion vocabulary
		int n = rlx_get_history_length(h);
		using_history();
		for(int j = 0; j < n; j++) {
			HIST_ENTRY* entry = history_get(history_base + j);
			if( entry && entry->line && *entry->line ) {
				vocabSize++;
			}
		}
	}
	// count vocabulary extensions
	for(rlx_vocabulary_extension_t* ext = rlx->vocabularyExtensions; ext; ext = ext->next) {
		vocabSize++;
	}
	// add one for the NULL terminator
	vocabSize++;

	vocab = malloc(sizeof(char*) * vocabSize);
	int i = 0;
	if( rlx->options & RLX_OPT_AUTOCOMPLETE_COMMANDS ) {
		// registered commands
		for( rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
			vocab[i++] = strdup(cmd->cmd.command);
		}
	}
	if( rlx->options & RLX_OPT_AUTOCOMPLETE_HISTORY ) {
		// history entries
		using_history();
		int n = rlx_get_history_length(h);
		for(int j = 0; j < n; j++) {
			HIST_ENTRY* entry = history_get(history_base + j);
			if( ! entry || ! entry->line || ! *entry->line ) continue;
			vocab[i++] = strdup(entry->line);
		}
	}
	for(rlx_vocabulary_extension_t* ext = rlx->vocabularyExtensions; ext; ext = ext->next) {
		vocab[i++] = strdup(ext->entry);
	}
	vocab[i] = 0; // NULL terminate the vocabulary array

	return vocab;
}

/**
	@brief Free the memory allocated for the completion vocabulary.
	@param vocab The completion vocabulary to free.
*/
static void rlx_free_completion_vocabulary(char** vocab) {
	if( ! vocab ) return;
	for(char** p = vocab; *p; p++) {
		free(*p);
	}
	free(vocab);
}

/**
	@brief Custom completion function for readline_ex.
	@param text The text to complete.
	@param start The start position of the text.
	@param end The end position of the text.
	@return An array of completion matches.
*/
static char** rlx_custom_completion(const char* text, int start, int end) {
	(void)start;
	(void)end;
	rl_attempted_completion_over = 1;
	return rl_completion_matches(text, rlx_custom_completion_generator);
}

static char* rlx_custom_completion_generator(const char* text, int state) {
	static size_t vocabIndex = 0;
	static size_t textLen = 0;

	if( state == 0 ) {
		// first call, build the completion vocabulary
		if( rlxStatic.ownsCompletionVocabulary ) {
			rlx_free_completion_vocabulary(rlxStatic.completionVocabulary);
			rlxStatic.completionVocabulary = rlx_build_completion_vocabulary((rlx_t)&rlxStatic);
		}
		vocabIndex = 0;
		textLen = strlen(text);
	}

	// search through the vocabulary for the next matching the input text
	while( rlxStatic.completionVocabulary[vocabIndex] ) {
		const char* candidate = rlxStatic.completionVocabulary[vocabIndex++];
		if( strncmp(candidate, text, textLen) == 0 ) {
			return strdup(candidate);
		}
	}

	return 0;
}

/**
	@brief Set a custom autocomplete vocabulary for the readline_ex session.
	@param h The readline_ex session handle.
	@param vocab The custom autocomplete vocabulary, or NULL to use the default vocabulary.
*/
void rlx_set_autocomplete_vocabulary(rlx_t h, char** vocab) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	if( rlx->ownsCompletionVocabulary ) {
		// dispose of the old vocabulary if we own it before replacing it with the new one
		rlx_free_completion_vocabulary(rlx->completionVocabulary);
		rlx->completionVocabulary = 0;
	}
	rlx->completionVocabulary = vocab;
	rlx->ownsCompletionVocabulary = vocab == 0;
}

/**
 * @brief Add a new entry to the autocomplete vocabulary.
 * @param h The readline_ex session handle.
 * @param entry The entry to add to the autocomplete vocabulary.
 */
void rlx_add_autocomplete_vocabulary_entry(rlx_t h, const char* entry) {
	rlx_internal_t* rlx = (rlx_internal_t*)h;
	ASSERT(rlx);
	if( ! entry || ! *entry ) return;

	// if we don't own the vocabulary, we cannot modify it
	if( ! rlx->ownsCompletionVocabulary ) return;

	// create a new vocabulary extension entry
	rlx_vocabulary_extension_t* ext = malloc(sizeof(rlx_vocabulary_extension_t));
	ext->entry = strdup(entry);
	ext->next = rlx->vocabularyExtensions;
	rlx->vocabularyExtensions = ext;
}
