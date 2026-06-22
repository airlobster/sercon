#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <math.h>
#include "utils.h"

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

const char* getHomeDir() {
	const char* homeDir = getenv("HOME");
	if( ! homeDir ) {
		homeDir = getpwuid(getuid())->pw_dir;
	}
	return homeDir;
}

int numdigits(long long v) {
	if (v == 0) return 1;
	return ceil(log10l(llabs(v)));
}
