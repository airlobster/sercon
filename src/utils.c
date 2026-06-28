#include <string.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <glob.h>
#include <math.h>
#include "utils.h"
#include "r_array.h"

/**
 * @brief Get the current time.
 * @param t The structure to store the current time.
 * @return 0 on success, non-zero on failure.
 */
int now(cal_time_t* t) {
	struct timespec ts;
	struct tm tm;
	clock_gettime(CLOCK_REALTIME, &ts);
	localtime_r(&ts.tv_sec, &tm);
	t->day = tm.tm_mday;
	t->month = tm.tm_mon + 1;
	t->year = tm.tm_year + 1900;
	t->hours = tm.tm_hour;
	t->minutes = tm.tm_min;
	t->seconds = tm.tm_sec;
	t->milliseconds = ts.tv_nsec / 1000000;
	return 0;
}

/**
 * @brief Get the home directory of the current user.
 * @return The path to the home directory.
 */
const char* getHomeDir() {
	const char* homeDir = getenv("HOME");
	if( ! homeDir ) {
		homeDir = getpwuid(getuid())->pw_dir;
	}
	return homeDir;
}

/**
 * @brief Get the number of digits in an integer.
 * @param v The integer.
 * @param base The numerical base. (e.g., 10 for decimal, 16 for hexadecimal. If 0, defaults to 10.)
 * @return The number of digits.
 */
int numdigits(long long v, unsigned short base) {
	if( v == 0 ) return 1;
	return ceil(logl(llabs(v)) / logl(base ? base : 10));
}


/**
	@brief Find the start and end of the non-whitespace content in a string.
	@param s The input string.
	@param start Pointer to store the start of the content.
	@param end Pointer to store the end of the content.
	@return The length of the content.
*/
int strnetcontent(char* s, char** start, char** end) {
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

#ifdef _DEBUG_
void debug_msg(const char* file, int line, const char* fmt, ...) {
	va_list args;
	char *msg = 0;

	fprintf(stderr, "[%s:%d] ", file, line);

	va_start(args, fmt);
	vasprintf(&msg, fmt, args);
	va_end(args);

	// trim leading and trailing whitespaces
	char *start, *end;
	strnetcontent(msg, &start, &end);
	*end = '\0'; // null-terminate at the last non-whitespace character

	fprintf(stderr, "%s\n", start);
	fflush(stderr);

	free(msg);
}
#endif

/**
 * @brief Perform a glob operation and invoke a callback for each matched path.
 * @param pattern The glob pattern.
 * @param callback The callback function to be called for each matched path.
 * @param userData User-defined data to be passed to the callback function.
 * @return int The number of matched paths.
 */
int cglob(const char* pattern, void(*callback)(const char* path, void* userData), void* userData) {
	ASSERT(pattern);
	ASSERT(callback);
	int n = 0;
	glob_t glob_result;
	memset(&glob_result, 0, sizeof(glob_result));
	int ret = glob(pattern, GLOB_TILDE | GLOB_NOSORT, NULL, &glob_result);
	if( ret == 0 ) {
		for( size_t i = 0; i < glob_result.gl_pathc; ++i ) {
			callback(glob_result.gl_pathv[i], userData);
			++n;
		}
	} else if( ret != GLOB_NOMATCH ) {
		DEBUG_MSG("Error occurred while globbing pattern: %s", pattern);
	}
	globfree(&glob_result);
	return n;
}

/**
 * @brief Parse a colon-separated list of paths and populate argc and argv.
 *
 * @param pathlist The colon-separated list of paths.
 * @param argc Pointer to store the number of paths.
 * @param argv Pointer to store the array of paths.
 * @return int The number of paths parsed.
 * @note The caller is responsible for freeing the memory allocated for argv and its contents.
 */
int parse_path_list(const char* pathlist, int* argc, char*** argv) {
	ASSERT(pathlist);
	ASSERT(argc);
	ASSERT(argv);
	*argc = 0;
	*argv = 0;
	r_array_t paths_array = r_array_create(100, free);
	const char* start = pathlist, *end = pathlist;
	while( *start ) {
		while( isspace(*start) ) {
			++start;
		}
		end = start;
		while( *end && *end != ':' ) {
			++end;
		}
		if( end > start ) {
			r_array_add(paths_array, strndup(start, end - start));
		}
		start = (*end) ? end + 1 : end;
	}
	*argc = r_array_size(paths_array);
	*argv = (char**)r_array_detach_elements(paths_array);
	r_array_destroy(paths_array);
	return *argc;
}


static struct termios originalTermios;
static void reset_termios(void) {
	DEBUG_MSG("Resetting terminal settings to original");
	tcsetattr(fileno(stdin), TCSANOW, &originalTermios);
}
static void on_exit_app(void) { reset_termios(); }
INITIALIZER(static void set_termios_scope(void)) {
	DEBUG_MSG("Initiating termios scope");
	tcgetattr(fileno(stdin), &originalTermios);
	atexit(on_exit_app);
}

#ifdef _DEBUG_
#include <sanitizer/asan_interface.h>
static void asan_callback(void) { reset_termios(); };
INITIALIZER(static void set_asan_callback(void)) {
	DEBUG_MSG("Setting ASAN death callback");
	__asan_set_death_callback(asan_callback);
}
#endif
