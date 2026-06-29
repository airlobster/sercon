#include <stdio.h>
#include <math.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "readline_ex.h"
#include "command.h"
#include "utils.h"

/**
 * @brief Node for a linked list of registered commands.
 */
typedef struct _rlx_command_node_t {
	/** Pointer to the next node in the linked list. */
	struct _rlx_command_node_t* next;
	/** The registered command. */
	rlx_registered_command_t cmd;
} rlx_command_node_t;

/**
 * @brief Internal readline context structure type.
 */
typedef struct _rlx_internal_t {
	/** Flag indicating whether the readline context has been initialized. */
	bool isInitialized;
	/** The callback function to handle input lines. */
	rlx_callback_t callback;
	/** User data to pass to the callback function and registered command handlers. */
	void* userData;
	/** Options for configuring the readline_ex session. */
	unsigned long options;
	/** The file path for the history file. */
	char* historyFilePath;
	/** The maximum number of history entries to keep. */
	size_t maxHistoryEntries;
	/** The buffer for the currently saved line. */
	char* savedLineBuffer;
	/** The linked list of registered commands. */
	rlx_command_node_t* commands;
	/** The vocabulary for autocompletion. */
	vocabulary_t completionVocabulary;
	/** Flag indicating whether the readline_ex session owns the completion vocabulary. */
	bool ownsCompletionVocabulary;
	/** Flag indicating whether the readline_ex session is paused. */
	bool isPaused;
} rlx_internal_t;

static rlx_internal_t rlxStatic = {0};

// forward local declarations
static char** rlx_custom_completion(const char* text, int start, int end);
static char* rlx_custom_completion_generator(const char* text, int state);
static void rlx_add_history_entry(rlx_t h, const char* line);
static void rlx_commit_history(rlx_t h);
static void rlx_load_history_into_autocomplete(rlx_t h);

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
	rlx->historyFilePath = makeHistoryFilePath(appname, historyContext);
	rlx->maxHistoryEntries = MAX(maxHistoryEntries, 10);
	rlx->savedLineBuffer = 0;
	rlx->commands = 0;
	rlx->completionVocabulary = vocab_create(0, 0);
	rlx->ownsCompletionVocabulary = true;
	rlx->isPaused = false;

	// if the option to persist history is enabled, we will load the history from the file on startup
	if( rlx->options & RLX_OPT_PERSIST_HISTORY ) {
		read_history(rlx->historyFilePath);
		if( rlx->options & RLX_OPT_AUTOCOMPLETE_HISTORY ) {
			// if the option to autocomplete from history is enabled,
			// we will load the history entries into the autocomplete vocabulary
			rlx_load_history_into_autocomplete(rlx);
		}
	}

	rl_callback_handler_install(prompt, readline_callback_wrapper);

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

	using_history();

	return (rlx_t)rlx;
}

/**
	@brief End the readline_ex session, cleaning up resources.
	@param rlx The readline_ex session handle.
*/
void rlx_end(rlx_t rlx) {
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);

	if( ! rlx || ! rlx->isInitialized ) return;

	rl_callback_handler_remove();

	rlx_commit_history(rlx);

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
	if( rlx->ownsCompletionVocabulary && rlx->completionVocabulary ) {
		vocab_destroy(rlx->completionVocabulary);
		rlx->completionVocabulary = 0;
	}

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
 * @param rlx The readline_ex session handle.
 * @param newPrompt The new prompt string to display.
 * @note: previous prompt string will not be freed because it is owned and managed
 * by the application itself.
 */
void rlx_change_prompt(rlx_t rlx, const char* newPrompt) {
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	ASSERT(newPrompt);
	if( rl_prompt && newPrompt && strcmp(rl_prompt, newPrompt) == 0 ) {
		// prompt is already set to the requested value, no need to change it
		return;
	}
	rl_set_prompt(newPrompt);
	rl_redisplay();
}

/**
	@brief Register commands with the readline_ex session.
	@param rlx The readline_ex session handle.
	@param commands An array of commands to register.
*/
void rlx_register_commands(rlx_t rlx, const rlx_registered_command_t* commands) {
	ASSERT(rlx);
	ASSERT(commands);
	for( const rlx_registered_command_t* rc = commands; rc && rc->command && rc->handler; rc++ ) {
		if( rlx_get_command(rlx, rc->command) ) continue;
		rlx_command_node_t* cmd = malloc(sizeof(rlx_command_node_t));
		cmd->cmd = *rc;
		cmd->next = rlx->commands;
		rlx->commands = cmd;
		if( rlx->ownsCompletionVocabulary && rlx->completionVocabulary && (rlx->options & RLX_OPT_AUTOCOMPLETE_COMMANDS) ) {
			vocab_add_word(rlx->completionVocabulary, rc->command);
		}
	}
}

/**
	@brief Get a registered command by its name.
	@param rlx The readline_ex session handle.
	@param command The name of the command to retrieve.
	@return A pointer to the registered command, or NULL if not found.
*/
const rlx_registered_command_t* rlx_get_command(rlx_t rlx, const char* command) {
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
	@param rlx The readline_ex session handle.
	@param line The command line to process.
	@return true if a registered command was found and executed, false otherwise.
*/
bool rlx_process_command(rlx_t rlx, const char* line) {
	char* expanded = 0;
	int argc=0;
	char** argv=0;
	const rlx_registered_command_t* cmd = 0;

	ASSERT(rlx);
	if( ! line || ! *line ) return false;

	int expansionResult = history_expand((char*)line, &expanded);
	if( expansionResult < 0 || expansionResult == 2 ) {
		// history expansion failed, or should be ignored
		free(expanded);
		return false;
	} else if( expansionResult > 0 ) {
		line = expanded;
	}

	// // we convert the command line into argc/argv format for easier parsing by command handlers,
	// // and then free the argv array after processing.
	if( parse_command_line(line, &argc, &argv) > 0 ) {
		if( (cmd = rlx_get_command(rlx, argv[0])) != 0 ) {
			cmd->handler(rlx, cmd, argc, (const char**)argv, rlx->userData);
		}
		free_command_args(argc, argv);
	}

	if( expanded ) {
		free(expanded);
	}

	if( ! (rlx->options & RLX_OPT_HISTORY_ALLOW_DUPLICATES) ) {
		// add history entry while avoiding duplications
		HIST_ENTRY* lastEntry = previous_history();
		if( ! lastEntry || strcmp(lastEntry->line, line) != 0 ) {
			rlx_add_history_entry(rlx, line);
		}
	} else {
		rlx_add_history_entry(rlx, line);
	}

	return cmd != 0;
}

/**
	@brief Add a line to the command history for the readline_ex session.
	@param rlx The readline_ex session handle.
	@param line The line to add to the history.
*/
static void rlx_add_history_entry(rlx_t rlx, const char* line) {
	ASSERT(rlx);
	if( ! line || ! *line ) return;
	add_history(line);
	// add the new history entry to the autocomplete vocabulary if we own it and the option is enabled
	if( rlx->ownsCompletionVocabulary && rlx->completionVocabulary && (rlx->options & RLX_OPT_AUTOCOMPLETE_HISTORY) ) {
		vocab_add_word(rlx->completionVocabulary, line);
	}
}

/**
	@brief Pause the readline_ex session, saving the current input line and prompt.
	@param rlx The readline_ex session handle.
*/
void rlx_pause(rlx_t rlx) {
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
	@param rlx The readline_ex session handle.
	@param redisplayPrompt Whether to redisplay the prompt and input line.
*/
void rlx_resume(rlx_t rlx, bool redisplayPrompt) {
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
	@brief Process input for the readline_ex session.
	@param rlx The readline_ex session handle.
*/
void rlx_process_input(rlx_t rlx) {
	(void)rlx;
	ASSERT(rlx);
	if( isatty(fileno(stdin)) ) {
		// interactive mode. let readline do its thing
		rl_callback_read_char();
	} else {
		// in non-interactive mode, we override the readline's state-machine, read the content ourselves
		// and call the callback directly for each line of input, until we reach EOF.
		for(;;) {
			char* line = 0;
			size_t len = 0;
			ssize_t read = 0;
			if( (read = getline(&line, &len, stdin)) == -1 ) {
				free(line); // !! getline allocates a buffer even on EOF !!
				readline_callback_wrapper(0);
				break;
			}
			readline_callback_wrapper(line);
			// freeing the line buffer is handled by readline_callback_wrapper!!
		}
	}
}

/**
	@brief Reset the command history for the readline_ex session.
	@param rlx The readline_ex session handle.
*/
void rlx_reset_history(rlx_t rlx) {
	(void)rlx;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	rl_clear_history();
}

/**
 * @brief Get the number of entries in the command history for the readline_ex session.
 * @param rlx The readline_ex session handle.
 * @return int The number of entries in the command history.
 */
int rlx_get_history_length(rlx_t rlx) {
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
	@param rlx The readline_ex session handle.
*/
static void rlx_commit_history(rlx_t rlx) {
	ASSERT(rlx);
	if( rlx->options & RLX_OPT_PERSIST_HISTORY ) {
		write_history(rlx->historyFilePath);
		history_truncate_file(rlx->historyFilePath, rlx->maxHistoryEntries);
	}
}

/**
	@brief Print the command history for the readline_ex session.
	@param rlx The readline_ex session handle.
*/
void rlx_print_history(rlx_t rlx) {
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	using_history();
	int n = rlx_get_history_length(rlx);
	if( n <= 0 ) return;
	for(int i=0; i < n; i++) {
		HIST_ENTRY* entry = history_get(history_base + i);
		if( ! entry || ! entry->line ) continue;
		printf("%*d: %s\n", numdigits(history_base + n, 0), history_base + i, entry->line);
	}
}

static int compare_commands(const void* a, const void* b) {
	const rlx_registered_command_t* cmdA = *(const rlx_registered_command_t**)a;
	const rlx_registered_command_t* cmdB = *(const rlx_registered_command_t**)b;
	return strcasecmp(cmdA->command, cmdB->command);
}
/**
	@brief Print the registered commands for the readline_ex session.
	@param rlx The readline_ex session handle.
*/
void rlx_print_registered_commands(rlx_t rlx) {
	int i = 0;
	int maxLen = 0;

	ASSERT(rlx);
	ASSERT(rlx->isInitialized);

	if( ! rlx->commands || ! rlx->commands->cmd.command ) return;

	// count the number of registered commands and find the maximum command length for formatting
	int nCommands = 0;
	for( rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
		++nCommands;
		int len = strlen(cmd->cmd.command);
		if( len > maxLen ) maxLen = len;
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
		printf("  %-*s - %s\n",
			maxLen,
			cmd->command,
			cmd->description ? cmd->description : "(no description)");
	}

	// cleanup
	free(commands);
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

/**
 * @brief Custom completion generator for readline_ex.
 * @param text The text to complete.
 * @param state The state of the completion.
 * @return The next completion match.
 */
static char* rlx_custom_completion_generator(const char* text, int state) {
	static const char** completionList = 0;
	static size_t vocabIndex = 0;
	static size_t textLen = 0;

	if( state == 0 ) {
		ASSERT(rlxStatic.completionVocabulary);
		// first call, build the completion words list
		completionList = (const char**)vocab_get_words(rlxStatic.completionVocabulary);
		ASSERT(completionList);
		vocabIndex = 0;
		textLen = strlen(text);
	}

	// search through the vocabulary for the next matching the input text
	while( completionList[vocabIndex] ) {
		const char* candidate = completionList[vocabIndex++];
		if( strncmp(candidate, text, textLen) == 0 ) {
			return strdup(candidate);
		}
	}

	return 0;
}

/**
	@brief Set a custom autocomplete vocabulary for the readline_ex session.
	@param rlx The readline_ex session handle.
	@param vocab The custom autocomplete vocabulary, or NULL to use the default vocabulary.
*/
void rlx_set_autocomplete_vocabulary(rlx_t rlx, vocabulary_t vocab) {
	ASSERT(rlx);
	if( rlx->ownsCompletionVocabulary ) {
		// dispose of the old vocabulary if we own it before replacing it with the new one
		vocab_destroy(rlx->completionVocabulary);
		rlx->completionVocabulary = 0;
	}
	rlx->completionVocabulary = vocab;
	rlx->ownsCompletionVocabulary = vocab == 0;
}

/**
 * @brief Add a new entry to the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 * @param entry The entry to add to the autocomplete vocabulary.
 * @return true if the entry was added successfully, false otherwise.
 */
bool rlx_add_autocomplete_vocabulary_entry(rlx_t rlx, const char* entry) {
	ASSERT(rlx);
	ASSERT(entry && *entry);
	if( ! entry || ! *entry ) return false;

	// if we don't own the vocabulary, we cannot modify it
	if( ! rlx->ownsCompletionVocabulary || ! rlx->completionVocabulary ) return false;

	return vocab_add_word(rlx->completionVocabulary, entry);
}

/**
 * @brief Load the readline history into the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 */
static void rlx_load_history_into_autocomplete(rlx_t rlx) {
	ASSERT(rlx);
	ASSERT(rlx->completionVocabulary && rlx->ownsCompletionVocabulary);
	using_history();
	int n = rlx_get_history_length(rlx);
	for(int j = 0; j < n; j++) {
		HIST_ENTRY* entry = history_get(history_base + j);
		if( ! entry || ! entry->line || ! *entry->line ) continue;
		vocab_add_word(rlx->completionVocabulary, entry->line);
	}
}

#ifdef _DEBUG_
/**
 * @brief Print the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 */
void rlx_print_autocomplete_vocabulary(rlx_t rlx) {
	ASSERT(rlx);
	if( rlx->completionVocabulary ) {
		vocab_print(rlx->completionVocabulary);
	} else {
		printf("No autocomplete vocabulary is set.\n");
	}
}
#endif

/**
 * @brief Make a safe prompt for readline by marking ANSI escape sequences.
 * @param prompt The original prompt string.
 * @param outSafePrompt The output safe prompt string.
 */
void rlx_make_safe_prompt(const char* prompt, char** outSafePrompt) {
	static const size_t INITIAL_OUTPUT_BUFFER_SIZE = 128;
	size_t currOutputSize = 0;
	char *safePrompt = 0;
	size_t pos = 0;;
	int ansiEscapeSequence = 0;
	ASSERT(outSafePrompt);
	if( ! prompt ) {
		*outSafePrompt = 0;
		return;
	}
	for(const char *c = prompt; *c; c++) {
		if( pos >= currOutputSize ) {
			// resize the output buffer if we exceed the current size
			currOutputSize = MAX(currOutputSize * 2, INITIAL_OUTPUT_BUFFER_SIZE);
			safePrompt = realloc(safePrompt, currOutputSize + 1);
			if( ! safePrompt ) {
				DEBUG_MSG("Failed to allocate memory for safe prompt");
				*outSafePrompt = 0;
				return;
			}
		}
		if( *c == '\033' ) {
			ASSERT(ansiEscapeSequence == 0);
			++ansiEscapeSequence;
			safePrompt[pos++] = '\001'; // mark the start of an ANSI escape sequence for readline
			safePrompt[pos++] = *c;
		} else if( ansiEscapeSequence ) {
			safePrompt[pos++] = *c;
			if( isalpha(*c) ) {
				safePrompt[pos++] = '\002'; // mark the end of an ANSI escape sequence for readline
				--ansiEscapeSequence;
				ASSERT(ansiEscapeSequence >= 0);
			}
		} else {
			safePrompt[pos++] = *c;
		}
	}
	safePrompt[pos] = '\0';
	*outSafePrompt = safePrompt;
}
