#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "strtok_iter.h"
#include "utils.h"

/**
 * @brief Options for the strtok iterator.
 *
 * This struct holds the options for the strtok iterator, including the string to tokenize
 * and the delimiters to use for tokenization.
 */
typedef struct {
	char* str; /**< The string to be tokenized. */
	char* delim; /**< The delimiters to use for tokenization. */
} strtok_iter_options_t;

/**
 * @brief State for the strtok iterator.
 *
 * This struct holds the state of the strtok iterator, including the current position
 * in the string, the end of the string, and a lookup table for delimiters.
 */
typedef struct {
	strtok_iter_options_t* options; /**< The options for the strtok iterator. */
	const char *start, *end; /**< Pointers to the start and end of the current token. */
	bool ascii[127]; /**< Lookup table for delimiters. */
} strtok_iter_state_t;

/**
 * @brief Destructor for the strtok iterator state.
 *
 * This function frees the resources associated with the strtok iterator state.
 *
 * @param state The state to be destroyed.
 */
static void strtok_iter_state_dtor(void* state) {
	if( ! state ) return;

	strtok_iter_state_t* s = (strtok_iter_state_t*)state;
	if( s->options ) {
		FREE(s->options->delim);
		FREE(s->options->str);
		FREE(s->options);
	}
	FREE(s);
}

/**
 * @brief Generate the next token from the strtok iterator.
 *
 * This function generates the next token from the string based on the specified delimiters.
 * It updates the state of the iterator and returns the next token.
 *
 * @param state The current state of the iterator.
 * @param done A pointer to an integer that indicates whether the iteration is done.
 * @param context The context for the iterator, which includes the options for tokenization.
 * @return void* The next token, or NULL if there are no more tokens.
 */
static void *strtok_iter_next(void** state, int* done, void* context) {
	ASSERT(state != NULL);
	ASSERT(done != NULL);
	ASSERT(context != NULL);

	strtok_iter_state_t* ctx = (strtok_iter_state_t*)(*state);

	// first call, initialize the state
	if( ! ctx ) {
		ctx = MALLOC(sizeof(strtok_iter_state_t));
		if( ! ctx ) {
			*done = 1;
			return NULL;
		}
		ctx->options = (strtok_iter_options_t*)context;
		ctx->start = ctx->options->str;
		ctx->end = ctx->options->str;
		// init delim lookup table
		memset(ctx->ascii, 0, sizeof(ctx->ascii));
		for(const char* d = ctx->options->delim; *d; ++d) {
			ASSERT((size_t)(*d) < array_size(ctx->ascii));
			ctx->ascii[(int)*d] = true;
		}
		*state = ctx;
	}

	while( *ctx->start ) {
		char* token = NULL;
		// skip leading delimiters
		while( *ctx->start && ctx->ascii[(int)*ctx->start] ) {
			++ctx->start;
		}
		if( ! *ctx->start ) {
			break;
		}
		// seek end of token
		ctx->end = ctx->start;
		while( *ctx->end && ! ctx->ascii[(int)*ctx->end] ) {
			++ctx->end;
		}
		if( ctx->end > ctx->start ) {
			token = STRNDUP(ctx->start, ctx->end - ctx->start);
		}
		ctx->start = ctx->end; // move start to end for next token
		if( token ) {
			return token;
		}
	}

	*done = 1;
	return NULL;
}

/**
 * @brief Create a strtok iterator.
 *
 * This function creates an iterator that tokenizes a string based on the specified delimiters.
 *
 * @param str The string to be tokenized.
 * @param delim The delimiters to use for tokenization.
 * @return iterator_t The created iterator.
 */
iterator_t strtok_iter(const char *str, const char *delim) {
	ASSERT(str != NULL);
	ASSERT(delim != NULL);
	// allocate options struct and copy arguments into it, so that the iterator
	// can be used after the original strings go out of scope
	strtok_iter_options_t *options = MALLOC(sizeof(strtok_iter_options_t));
	if( ! options ) {
		return NULL;
	}
	options->delim = STRDUP(delim);
	options->str = STRDUP(str);
	return iterator_init(strtok_iter_next, strtok_iter_state_dtor, options);
}
