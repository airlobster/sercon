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
	glob_t glob_result;
	size_t i;
} cglob_context_t;

/**
 * @brief Creates a new glob iterator for the given pattern and options.
 * @param pattern The glob pattern to match files against.
 * @param options Bitmask of cglob_file_type_t values specifying which file types to include.
 * @return cglob_iterator_t - A pointer to the newly created iterator, or NULL on failure.
 */
cglob_iterator_t globIterator(const char* pattern, unsigned long options) {
	cglob_context_t* ctx = malloc(sizeof(cglob_context_t));
	if( ! ctx ) return NULL;
	ctx->pattern = pattern;
	ctx->options = options;
	memset(&ctx->glob_result, 0, sizeof(glob_t));
	ctx->i = 0;
	int ret = glob(pattern, GLOB_TILDE | GLOB_NOSORT, NULL, &ctx->glob_result);
	if( ret != 0 && ret != GLOB_NOMATCH ) {
		DEBUG_MSG("Error occurred while globbing pattern: %s", pattern);
		free(ctx);
		return NULL;
	}
	return (cglob_iterator_t)ctx;
}

/**
 * @brief Retrieves the next matching file path from the glob iterator.
 * @param iterator The glob iterator to retrieve the next path from.
 * @return const char* - The next matching file path, or NULL if there are no more matches.
 */
const char* nextGlob(cglob_iterator_t iterator) {
	ASSERT(iterator);
	cglob_context_t* ctx = (cglob_context_t*)iterator;
	if( ! ctx ) return NULL;
	while( ctx->i < ctx->glob_result.gl_pathc ) {
		const char* path = ctx->glob_result.gl_pathv[ctx->i++];
		struct stat st;
		if( stat(path, &st) != 0 ) {
			DEBUG_MSG("Failed to stat file: %s (%s)", path, strerror(errno));
			continue;
		}
		bool pass = false;
		pass |= (ctx->options & CGLOB_FILE_REGULAR) && S_ISREG(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_DIRECTORY) && S_ISDIR(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_SYMLINK) && S_ISLNK(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_CHAR_DEVICE) && S_ISCHR(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_BLOCK_DEVICE) && S_ISBLK(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_FIFO) && S_ISFIFO(st.st_mode);
		pass |= (ctx->options & CGLOB_FILE_SOCKET) && S_ISSOCK(st.st_mode);
		if( pass ) return path;
	}
	// No more matching paths
	return NULL;
}

/**
 * @brief Frees the resources associated with the glob iterator.
 * @param iterator The glob iterator to free.
 */
void freeGlobIterator(cglob_iterator_t iterator) {
	ASSERT(iterator);
	if( ! iterator ) return;
	cglob_context_t* ctx = (cglob_context_t*)iterator;
	globfree(&ctx->glob_result);
	free(ctx);
}
