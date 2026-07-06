#include <glob.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include "d_array.h"
#include "cglob.h"
#include "utils.h"

typedef struct {
	d_array_t patterns;  // Array of patterns to match
	unsigned long options;
} cglob_options_t;

typedef struct {
	cglob_options_t* options;
	glob_t glob_result;
	size_t current_pattern;
	size_t iResult;
} cglob_state_t;

/**
 * @brief Frees the resources associated with the glob iterator state.
 * 
 * @param state Pointer to the iterator state to be freed.
 */
static void cglob_free(void* state) {
	if( ! state ) return;
	globfree(&((cglob_state_t*)state)->glob_result);
	d_array_destroy(((cglob_state_t*)state)->options->patterns);
	free(((cglob_state_t*)state)->options);
	free(state);
}

/**
 * @brief Get the next matching file from the glob iterator.
 * 
 * @param state Pointer to the iterator state.
 * @param done Pointer to an integer that will be set to 1 when iteration is complete.
 * @param context Pointer to the iterator context (cglob_options_t).
 * @return void* Pointer to the next matching file path, or NULL if iteration is complete.
 */
static void* cglob_next(void** state, int* done, void* context) {
	if( ! state || ! done ) return NULL;

	cglob_state_t* ctx = (cglob_state_t*)*state;

	// first time - initialize state
	if( ! *state ) {
		ctx = malloc(sizeof(cglob_state_t));
		ctx->current_pattern = 0;
		ctx->iResult = 0;
		ctx->options = (cglob_options_t*)context;
		*state = ctx;
	}

	// for each pattern...
	while( ctx->current_pattern < d_array_size(ctx->options->patterns) ) {
		const char* pattern = d_array_get(ctx->options->patterns, ctx->current_pattern);
		if( ctx->current_pattern ) {
			// free previous glob results before starting a new glob operation
			globfree(&ctx->glob_result);
		}
		// perform globbing for the current pattern
		int ret = glob(pattern, GLOB_TILDE | GLOB_NOSORT, NULL, &ctx->glob_result);
		if( ret != 0 && ret != GLOB_NOMATCH ) {
			DEBUG_MSG("Error occurred while globbing pattern: %s", pattern);
			*done = 1;
			return NULL;
		}
		// iterate through the results of the globbing operation
		while( ctx->iResult < ctx->glob_result.gl_pathc ) {
			const char* path = ctx->glob_result.gl_pathv[ctx->iResult++];
			struct stat st;
			if( stat(path, &st) != 0 ) {
				DEBUG_MSG("Failed to stat file: %s (%s)", path, strerror(errno));
				continue;
			}
			bool pass = false;
			pass |= (ctx->options->options & CGLOB_FILE_REGULAR) && S_ISREG(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_DIRECTORY) && S_ISDIR(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_SYMLINK) && S_ISLNK(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_CHAR_DEVICE) && S_ISCHR(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_BLOCK_DEVICE) && S_ISBLK(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_FIFO) && S_ISFIFO(st.st_mode);
			pass |= (ctx->options->options & CGLOB_FILE_SOCKET) && S_ISSOCK(st.st_mode);
			if( pass ) {
				return (void*)path;
			}
		}
		ctx->current_pattern++;
		ctx->iResult = 0;
	}

	*done = 1;

	return NULL;
}

/**
 * @brief Creates a glob iterator for the specified patterns and options.
 * 
 * @param patterns Array of patterns to match (NULL-terminated).
 * @param options Options for filtering the matched files.
 * @return iterator_t The initialized iterator.
 */
iterator_t cglob_iterator(const char* patterns[], unsigned long options) {
	// make a copy of the patterns array to ensure it remains valid for the lifetime of the iterator
	d_array_t patterns_array = d_array_create(0, free);
	for(const char** p = patterns; *p; ++p) {
		d_array_add(patterns_array, strdup(*p));
	}
	cglob_options_t* opt = malloc(sizeof(cglob_options_t));
	if( ! opt ) {
		DEBUG_MSG("Failed to allocate memory for cglob_options_t");
		d_array_destroy(patterns_array);
		return NULL;
	}
	opt->patterns = patterns_array;
	opt->options = options;
	return iterator_init(cglob_next, cglob_free, opt);
}
