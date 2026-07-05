#include <glob.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include "cglob.h"
#include "utils.h"

typedef struct {
	const char* pattern;
	unsigned long options;
} cglob_options_t;

typedef struct {
	cglob_options_t* options;
	glob_t glob_result;
	size_t i;
} cglob_state_t;

static void cglob_free(void* state) {
	if( ! state ) return;
	globfree(&((cglob_state_t*)state)->glob_result);
	free(((cglob_state_t*)state)->options);
	free(state);
}

static void* cglob_next(void** state, int* done, void* context) {
	if( ! state || ! done ) return NULL;

	cglob_state_t* ctx = (cglob_state_t*)*state;

	// first time - initialize state
	if( ! *state ) {
		ctx = malloc(sizeof(cglob_state_t));
		ctx->i = 0;
		ctx->options = (cglob_options_t*)context;
		int ret = glob(ctx->options->pattern, GLOB_TILDE | GLOB_NOSORT, NULL, &ctx->glob_result);
		if( ret != 0 && ret != GLOB_NOMATCH ) {
			DEBUG_MSG("Error occurred while globbing pattern: %s", ctx->options->pattern);
			*done = 1;
			cglob_free(ctx);
			return NULL;
		}
		*state = ctx;
	}

	while( ctx->i < ctx->glob_result.gl_pathc ) {
		const char* path = ctx->glob_result.gl_pathv[ctx->i++];
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

	*done = 1;

	return NULL;
}

iterator_t cglob_iterator(const char* pattern, unsigned long options) {
	cglob_options_t* opt = malloc(sizeof(cglob_options_t));
	if( ! opt ) {
		DEBUG_MSG("Failed to allocate memory for cglob_options_t");
		return NULL;
	}
	opt->pattern = pattern;
	opt->options = options;
	return iterator_init(cglob_next, cglob_free, opt);
}
