#include <string.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <math.h>
#include "utils.h"
#include "mem.h"

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
 * @brief Check if a given path is a directory.
 * @param path The path to check.
 * @return true if the path is a directory, false otherwise.
 */
bool isDirectory(const char* path) {
	ASSERT(path && *path);
	struct stat s;
	if( stat(path, &s) == 0 ) {
		return S_ISDIR(s.st_mode);
	}
	return false;
}

/**
 * @brief Get the number of digits in an integer.
 * @param v The integer.
 * @param base The numerical base. (e.g., 10 for decimal, 16 for hexadecimal. If 0, defaults to 10).
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
void debug_msg(const char* file, int line, const char* func, const char* fmt, ...) {
	va_list args;
	char *msg = NULL;

	va_start(args, fmt);
	vasprintf(&msg, fmt, args);
	va_end(args);

	fprintf(stderr, "[%s@%s:%d] %s\n", func, file, line, msg ? msg : "");
	fflush(stderr);

	if( msg ) {
		FREE(msg);
	}
}
#endif

/**
 * @brief Parse a connection string in the format "PORT{:BAUD}".
 * @param connectionString The input connection string.
 * @param portName Buffer to store the extracted port name.
 * @param portNameSize Size of the portName buffer.
 * @param baudRate Pointer to store the extracted baud rate. Defaults to 9600 if not specified or invalid.
 * @return The number of components successfully parsed (1 for port, 2 for port and baud), or -1 on error.
 */
int parseConnectionString(const char* connectionString, char* portName, size_t portNameSize, int* baudRate) {
	ASSERT(connectionString);
	ASSERT(portName);
	ASSERT(baudRate);
	if( ! connectionString || ! portName || ! baudRate ) return -1;
	int n = 0;
	*baudRate = 0;
	const char *pRd=connectionString;
	char *pWr = portName;
	// Skip leading whitespace
	while( isspace(*pRd) ) pRd++;
	while( *pRd && *pRd != ':' && ! isspace(*pRd) && (size_t)(pWr - portName) < (portNameSize - 1) ) {
		*pWr++ = *pRd++;
	}
	*pWr = '\0'; // null-terminate the port name
	n += (pWr - portName) ? 1 : 0; // increment n if port name was found
	while( *pRd && (isspace(*pRd) || *pRd == ':') ) pRd++; // skip whitespace after port name
	// Parse baud rate if present
	while( isdigit(*pRd) ) {
		*baudRate = (*baudRate * 10) + (*pRd - '0');
		pRd++;
	}
	if( *baudRate >= 9600 ) {
		n += 1; // increment n if baud rate was found
	} else {
		*baudRate = 9600; // default baud rate
	}
	return n;
}


static struct termios originalTermios;
static void reset_termios(void) {
	DEBUG_MSG("Resetting termios changes");
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
