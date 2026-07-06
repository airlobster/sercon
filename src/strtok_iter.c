#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "strtok_iter.h"
#include "utils.h"

typedef struct {
	char* str;
	char* delim;
} strtok_iter_options_t;

typedef struct {
	strtok_iter_options_t* options;
	const char *start, *end;
	bool ascii[127];
} strtok_iter_state_t;

static void strtok_iter_state_dtor(void* state) {
	if( ! state ) return;

	strtok_iter_state_t* s = (strtok_iter_state_t*)state;
	if( s->options ) {
		free(s->options->delim);
		free(s->options->str);
		free(s->options);
	}
	free(s);
}

static void *strtok_iter_next(void** state, int* done, void* context) {
	ASSERT(state != NULL);
	ASSERT(done != NULL);
	ASSERT(context != NULL);

	strtok_iter_state_t* ctx = (strtok_iter_state_t*)(*state);

	// first call, initialize the state
	if( ! ctx ) {
		ctx = malloc(sizeof(strtok_iter_state_t));
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
			token = strndup(ctx->start, ctx->end - ctx->start);
		}
		ctx->start = ctx->end; // move start to end for next token
		if( token ) {
			return token;
		}
	}

	*done = 1;
	return NULL;
}

iterator_t strtok_iter(const char *str, const char *delim) {
	ASSERT(str != NULL);
	ASSERT(delim != NULL);
	// allocate options struct and copy arguments into it, so that the iterator
	// can be used after the original strings go out of scope
	strtok_iter_options_t *options = malloc(sizeof(strtok_iter_options_t));
	if( ! options ) {
		DEBUG_MSG("Failed to allocate memory for strtok_iter_options_t");
		return NULL;
	}
	options->delim = strdup(delim);
	options->str = strdup(str);
	return iterator_init(strtok_iter_next, strtok_iter_state_dtor, options);
}
