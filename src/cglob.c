#include <glob.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include "cglob.h"
#include "utils.h"

/**
 * @brief Perform a glob operation and invoke a callback for each matched path.
 * @param pattern The glob pattern.
 * @param options Options for the glob operation.
 * @param callback The callback function to be called for each matched path.
 * @param context User-defined data to be passed to the callback function.
 * @return int The number of matched paths.
 */
int cglob(const char* pattern, unsigned long options, cglob_callback_t callback, void* context) {
	ASSERT(pattern);
	ASSERT(callback);
	int n = 0;
	glob_t glob_result;
	memset(&glob_result, 0, sizeof(glob_result));
	int ret = glob(pattern, GLOB_TILDE | GLOB_NOSORT, NULL, &glob_result);
	if( ret == 0 ) {
		for( size_t i = 0; i < glob_result.gl_pathc; ++i ) {
			struct stat st;
			bool pass = false;
			if( stat(glob_result.gl_pathv[i], &st) != 0 ) {
				DEBUG_MSG("Failed to stat file: %s (%s)", glob_result.gl_pathv[i], strerror(errno));
				continue;
			}
			pass |= (options & CGLOB_FILE_REGULAR) && S_ISREG(st.st_mode);
			pass |= (options & CGLOB_FILE_DIRECTORY) && S_ISDIR(st.st_mode);
			pass |= (options & CGLOB_FILE_SYMLINK) && S_ISLNK(st.st_mode);
			pass |= (options & CGLOB_FILE_CHAR_DEVICE) && S_ISCHR(st.st_mode);
			pass |= (options & CGLOB_FILE_BLOCK_DEVICE) && S_ISBLK(st.st_mode);
			pass |= (options & CGLOB_FILE_FIFO) && S_ISFIFO(st.st_mode);
			pass |= (options & CGLOB_FILE_SOCKET) && S_ISSOCK(st.st_mode);
			if( ! pass ) continue;
			callback(glob_result.gl_pathv[i], context);
			++n;
		}
	} else if( ret != GLOB_NOMATCH ) {
		DEBUG_MSG("Error occurred while globbing pattern: %s", pattern);
	}
	globfree(&glob_result);
	return n;
}
