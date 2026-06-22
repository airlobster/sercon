#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <math.h>
#include "utils.h"

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
