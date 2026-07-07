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
#include "ansi.h"
#include "d_buffer.h"
#include "d_array.h"
#include "vocabulary.h"
#include "cglob.h"
#include "mem.h"

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
	void* context;
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
	/** Array of autocomplete callbacks. */
	d_array_t autocompleteCallbacks;
	/** The vocabulary for autocompletion. */
	vocabulary_t completionVocabulary;
	/** Flag indicating whether the readline_ex session is paused. */
	bool isPaused;
} rlx_internal_t;

static rlx_internal_t rlxStatic = {0};

// forward local declarations
static char** rlx_custom_completion(const char* text, int start, int end);
static char* rlx_custom_completion_generator(const char* text, int state);
static void rlx_add_history_entry(rlx_t h, const char* line);
static void rlx_commit_history(rlx_t h);

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
		char* historyContextCpy = STRDUP(historyContext);
		// normalize the history context by replacing any non-alphanumeric characters with underscores
		for(char* p = historyContextCpy; *p; p++) {
			if( ! isalnum(*p) ) {
				*p = '_'; // replace any non-alphanumeric characters with underscores to ensure a valid file name
			}
		}
		asprintf(&path, "%s/.%s.%s.rlx.history", homeDir, appname, historyContextCpy);
		FREE(historyContextCpy);
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
				rlx->callback((rlx_t)rlx, start, end - start, rlx->context);
			} else {
				// ignore empty lines (only whitespace)
			}
		} else {
			rlx->callback((rlx_t)rlx, line, strlen(line), rlx->context);
		}
		FREE(line); // free the line buffer after processing to avoid memory leaks
	} else {
		// EOF received (e.g., Ctrl+D). notify the callback with a NULL line
		rlx->callback((rlx_t)rlx, 0, 0, rlx->context);
	}
}

static int event_hook(void) {
	return 0;
}

/**
 * @brief Autocomplete callback for registered commands.
 * @param h The readline_ex session handle.
 * @param text The text to complete.
 * @param callback The callback function to add.
 */
static void autocomplete_commands_callback(rlx_t rlx, const char* text, void* context) {
	(void)context;
	(void)text;
	ASSERT(rlx);
	ASSERT(rlx->completionVocabulary);
	// add registered commands to the autocomplete vocabulary
	for(const rlx_command_node_t* cmd = rlx->commands; cmd; cmd = cmd->next ) {
		rlx_add_autocomplete_vocabulary_entry(rlx, cmd->cmd.command);
	}
}

/**
 * @brief Autocomplete callback for file names.
 * @param h The readline_ex session handle.
 * @param text The text to complete.
 * @param context User data passed to the callback function.
 */
static void autocomplete_files_callback(rlx_t rlx, const char* text, void* context) {
	(void)rlx;
	(void)context;
	char* wildcard = NULL;
	// the caller has to use '/' to trigger file completion (./ or /path/to/file),
	// otherwise we will not add any file names to the vocabulary
	if( ! text || ! strchr(text, '/') ) return;
	// create the glob wildcard pattern for matching files and directories
	asprintf(&wildcard, "%s*", text ? text : "");
	if( ! wildcard ) {
		DEBUG_MSG("Failed to allocate memory for wildcard string.");
		return;
	}
	iterator_t g = cglob_iter((const char*[]){wildcard, NULL},
					CGLOB_FILE_REGULAR | CGLOB_FILE_DIRECTORY | CGLOB_FILE_SYMLINK);
	_foreach(g, r) {
		char* word = NULL;
		const char* filename = (const char*)r.value;
		// add a trailing slash to directories
		asprintf(&word, "%s%s", filename, isDirectory(filename) ? "/" : "");
		if( ! word ) {
			DEBUG_MSG("Failed to allocate memory for autocomplete word.");
			continue;
		}
		rlx_add_autocomplete_vocabulary_entry(rlx, word);
		FREE(word);
	}
	FREE(wildcard);
}

/**
	@brief Begin a readline_ex session.
	@param appname The name of the application.
	@param prompt The prompt string to display.
	@param callback The callback function to handle input lines.
	@param maxHistoryEntries The maximum number of history entries to keep.
	@param historyContext The context for the history file (optional).
	@param options Options for configuring the readline_ex session.
	@param context User data to pass to the callback function and registered command handlers.
	@return A handle to the readline_ex session, or NULL on failure.
*/
rlx_t rlx_begin(
	const char* appname,
	const char* prompt,
	rlx_callback_t callback,
	size_t maxHistoryEntries,
	const char* historyContext,
	unsigned long options,
	void* context
) {
	rlx_internal_t* rlx = &rlxStatic;

	ASSERT(appname && *appname);
	ASSERT(callback);

	// allow only a single readline context since readline uses global state !!!
	if( rlx->isInitialized ) return 0;

	rlx->isInitialized = true;
	rlx->callback = callback;
	rlx->context = context;
	rlx->options = options;
	rlx->historyFilePath = makeHistoryFilePath(appname, historyContext);
	rlx->maxHistoryEntries = MAX(maxHistoryEntries, 10);
	rlx->savedLineBuffer = 0;
	rlx->commands = 0;
	rlx->autocompleteCallbacks = 0;
	rlx->completionVocabulary = 0;
	rlx->isPaused = false;

	history_max_entries = rlx->maxHistoryEntries;

	// if the option to persist history is enabled, we will load the history from the file on startup
	if( rlx->options & RLX_OPT_PERSIST_HISTORY ) {
		read_history(rlx->historyFilePath);
	}

	rl_callback_handler_install(prompt, readline_callback_wrapper);

	if( rlx->options & RLX_OPT_AUTOCOMPLETE_MASK ) {
		if( rlx->options & RLX_OPT_AUTOCOMPLETE_DEFAULT ) {
			rl_attempted_completion_function = 0; // use default readline filename completion
		} else {
			rlx->autocompleteCallbacks = d_array_create(0, 0);
			rlx->completionVocabulary = vocab_create(0, 0);
			rl_attempted_completion_function = rlx_custom_completion;
			if( rlx->options & RLX_OPT_AUTOCOMPLETE_COMMANDS ) {
				rlx_add_autocomplete_callback((rlx_t)rlx, autocomplete_commands_callback);
			}
			if( rlx->options & RLX_OPT_AUTOCOMPLETE_FILES ) {
				rlx_add_autocomplete_callback((rlx_t)rlx, autocomplete_files_callback);
			}
		}
		rl_bind_key('\t', rl_complete);
		rl_completer_word_break_characters = " \t\n\"'`@$><=;|&{(";
	} else {
		// no auto-complete
		rl_bind_key('\t', rl_insert);
	}

	using_history();

	rl_event_hook = event_hook;

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

	FREE(rlx->historyFilePath);
	rlx->historyFilePath = 0;

	if( rlx->savedLineBuffer ) {
		FREE(rlx->savedLineBuffer);
		rlx->savedLineBuffer = 0;
	}

	// free the registered commands linked list
	for(rlx_command_node_t* cmd = rlx->commands; cmd; ) {
		rlx_command_node_t* next = cmd->next;
		FREE(cmd);
		cmd = next;
	}

	// free auto-complete vocabulary
	if( rlx->completionVocabulary ) {
		vocab_destroy(rlx->completionVocabulary);
		rlx->completionVocabulary = 0;
	}

	// destroy the array of autocomplete callbacks
	d_array_destroy(rlx->autocompleteCallbacks);

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
 * @note previous prompt string will not be freed because it is owned and managed
 * by the application itself.
 */
void rlx_change_prompt(rlx_t rlx, const char* newPrompt) {
	(void)rlx;
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
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
		rlx_command_node_t* cmd = MALLOC(sizeof(rlx_command_node_t));
		cmd->cmd = *rc;
		cmd->next = rlx->commands;
		rlx->commands = cmd;
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
	if( expansionResult >= 0 ) {
		line = expanded;
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

	// // we convert the command line into argc/argv format for easier parsing by command handlers,
	// // and then free the argv array after processing.
	if( parse_command_line(line, &argc, &argv) > 0 ) {
		if( (cmd = rlx_get_command(rlx, argv[0])) != 0 ) {
			cmd->handler(rlx, cmd, argc, (const char**)argv, rlx->context);
		}
		free_command_args(argc, argv);
	}

	if( expanded ) {
		FREE(expanded);
	}

	return cmd != 0;
}

/**
	@brief Add a line to the command history for the readline_ex session.
	@param rlx The readline_ex session handle.
	@param line The line to add to the history.
*/
static void rlx_add_history_entry(rlx_t rlx, const char* line) {
	(void)rlx;
	ASSERT(rlx);
	if( ! line || ! *line ) return;
	add_history(line);
}

/**
	@brief Pause the readline_ex session, saving the current input line and prompt.
	@param rlx The readline_ex session handle.
*/
void rlx_pause(rlx_t rlx) {
	ASSERT(rlx);
	if( rlx->isPaused ) return; // if we're already paused, no need to do anything
	rlx->savedLineBuffer = STRDUP(rl_line_buffer); // save the current readline input so we can restore it after printing serial output
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
	FREE(rlx->savedLineBuffer);
	rlx->savedLineBuffer = 0;
	rlx->isPaused = false;
}

/**
	@brief Inject input into the readline_ex session.
	@param h The readline_ex session handle.
	@param input The input string to inject.
	@details Send a copy of the input string to the readline_ex session as if it was typed by the user.
*/
void rlx_inject_input(rlx_t h, const char* input) {
	(void)h;
	ASSERT(h);
	if( ! input ) return;
	readline_callback_wrapper(STRDUP(input));
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
				FREE(line); // !! getline allocates a buffer even on EOF !!
				readline_callback_wrapper(0);
				break;
			}
			readline_callback_wrapper(line);
			// freeing the line buffer is handled by readline_callback_wrapper!!
		}
	}
}

/**
	@brief Get the current input line from the readline_ex session.
	@param rlx The readline_ex session handle.
	@return The current input line, or NULL if there is no input line.
*/
const char* rlx_get_current_line(rlx_t h) {
	(void)h;
	ASSERT(h);
	return rl_line_buffer;
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
	FREE(historyState);
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
	const rlx_registered_command_t** commands = MALLOC(sizeof(rlx_registered_command_t*) * nCommands);
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
	FREE(commands);
}

/**
 * @brief Add an autocomplete callback.
 * @param h The readline_ex session handle.
 * @param callback The callback function to add.
 * @return true if the callback was added successfully, false otherwise.
 */
bool rlx_add_autocomplete_callback(rlx_t h, rlx_vocabulary_build_callback_t callback) {
	ASSERT(h);
	ASSERT(callback);
	ASSERT(h->completionVocabulary);
	ASSERT(h->autocompleteCallbacks);
	if( ! h->completionVocabulary || ! h->autocompleteCallbacks ) {
		DEBUG_MSG("Cannot add autocomplete callback: RLX_OPT_AUTOCOMPLETE_CUSTOM was not set.");
		return false;
	}
	d_array_add(h->autocompleteCallbacks, callback);
	return true;
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
 * @brief Rebuild the autocomplete vocabulary by invoking all registered autocomplete callbacks.
 * @param rlx The readline_ex session handle.
 * @param text The text to complete.
 */
void rlx_rebuild_completion_vocabulary(rlx_t rlx, const char* text) {
	ASSERT(rlx);
	if( ! rlx->completionVocabulary || ! rlx->autocompleteCallbacks ) {
		DEBUG_MSG("Cannot rebuild autocomplete vocabulary: RLX_OPT_AUTOCOMPLETE_CUSTOM was not set.");
		return;
	}
	vocab_reset(rlx->completionVocabulary);
	// invoke auto-complete chain of callbacks to build the vocabulary
	for(size_t i=0; i < d_array_size(rlx->autocompleteCallbacks); i++) {
		rlx_vocabulary_build_callback_t callback =
			(rlx_vocabulary_build_callback_t)d_array_get(rlx->autocompleteCallbacks, i);
		ASSERT(callback != NULL);
		callback((rlx_t)rlx, text, rlx->context);
	}
}

/**
 * @brief Custom completion generator for readline_ex.
 * @param text The text to complete.
 * @param state The state of the completion.
 * @return The next completion match.
 */
static char* rlx_custom_completion_generator(const char* text, int state) {
	rlx_internal_t* rlx = &rlxStatic;
	ASSERT(rlx->isInitialized);
	ASSERT(rlx->completionVocabulary);
	ASSERT(rlx->autocompleteCallbacks);

	static d_array_t completionList = 0;
	static size_t vocabIndex = 0;
	static size_t textLen = 0;

	// first call for a given completion, we need to build the list of possible completions
	if( state == 0 ) {
		rlx_rebuild_completion_vocabulary(rlx, text);
		// replace the previous completion list with the new one from the vocabulary
		if( completionList ) {
			// since this array has no dtor function set, only the array itself will be freed,
			// while leaving the elements (strings) intact, which is what we want since they are owned by the vocabulary.
			d_array_destroy(completionList);
			completionList = 0;
		}
		completionList = vocab_get_words(rlx->completionVocabulary);
		ASSERT(completionList);
		vocabIndex = 0;
		textLen = strlen(text);
	}

	// search through the vocabulary for the next matching the input text
	while( vocabIndex < d_array_size(completionList) ) {
		const char* candidate = (const char*)d_array_get(completionList, vocabIndex++);
		if( strncmp(candidate, text, textLen) == 0 ) {
			return STRDUP(candidate);
		}
	}

	return 0;
}

/**
 * @brief Add a new entry to the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 * @param entry The entry to add to the autocomplete vocabulary.
 * @return true if the entry was added successfully, false otherwise.
 */
bool rlx_add_autocomplete_vocabulary_entry(rlx_t rlx, const char* entry) {
	ASSERT(rlx);
	ASSERT(rlx->isInitialized);
	ASSERT(rlx->completionVocabulary);
	ASSERT(entry && *entry);
	if( ! entry || ! *entry ) return false;

	// if we don't own the vocabulary, we cannot modify it
	if( ! rlx->completionVocabulary ) {
		DEBUG_MSG("Cannot add autocomplete vocabulary entry: RLX_OPT_AUTOCOMPLETE_CUSTOM was not set.");
		return false;
	}

	return vocab_add_word(rlx->completionVocabulary, entry);
}

#ifdef _DEBUG_
/**
 * @brief Print the autocomplete vocabulary.
 * @param rlx The readline_ex session handle.
 */
void rlx_print_autocomplete_vocabulary(rlx_t rlx) {
	ASSERT(rlx);
	rlx_rebuild_completion_vocabulary(rlx, 0);
	if( rlx->completionVocabulary ) {
		vocab_print(rlx->completionVocabulary);
	} else {
		printf("No autocomplete vocabulary is set.\n");
	}
}
#endif

/**
 * @brief Utility function for creating a safe version of a readline prompt string
 * by marking ANSI escape sequences.
 * @param prompt The original prompt string.
 * @param outSafePrompt The output safe prompt string.
 * @note The caller is responsible for freeing the outSafePrompt string.
 * @details This function scans the input prompt string for ANSI escape sequences,
 * and wraps them with special markers (\001 and \002) so that readline can ignore
 * them when calculating the prompt length for cursor positioning.
 */
void rlx_make_safe_prompt(const char* prompt, char** outSafePrompt) {
#define PUSH(s)		ASSERT(statePos < array_size(state)); state[statePos++] = (s)
#define POP()			ASSERT(statePos > 0); --statePos
#define PEEK()		state[statePos-1]
#define UNGET()		--c
	typedef enum {
		STATE_NORMAL,
		STATE_ANSI,
		STATE_ANSI_END
	} state_t;
	state_t state[16] = {0};
	size_t statePos = 0;
	buffer_t buf = d_buffer_create(0);

	ASSERT(outSafePrompt);

	if( ! prompt ) {
		*outSafePrompt = 0;
		return;
	}

	PUSH(STATE_NORMAL);

	for(register const char *c = prompt; *c; c++) {
		ASSERT(statePos > 0);
		switch(PEEK()) {
			case STATE_NORMAL: {
				if( *c == '\033' ) {
					d_buffer_append(buf, "\001", 1);
					PUSH(STATE_ANSI);
					UNGET(); // reprocess this character in the STATE_ANSI state
					break;
				}
				d_buffer_append(buf, c, 1);
				break;
			}
			case STATE_ANSI: {
				d_buffer_append(buf, c, 1);
				if( IS_ANSI_END_CHAR(*c) ) {
					POP();
					PUSH(STATE_ANSI_END);
				}
				break;
			}
			case STATE_ANSI_END: {
				POP();
				d_buffer_append(buf, "\002", 1);
				UNGET(); // reprocess this character in the STATE_NORMAL state
				break;
			}
		} // end of switch
	} // end of for loop

	// if we ended in the STATE_ANSI_END state, we need to close the ANSI sequence with \002
	ASSERT(statePos > 0);
	if( PEEK() == STATE_ANSI_END ) {
		d_buffer_append(buf, "\002", 1);
	}

	*outSafePrompt = d_buffer_detach_data(buf);
	d_buffer_destroy(buf);

#undef PUSH
#undef POP
#undef PEEK
#undef UNGET
}
