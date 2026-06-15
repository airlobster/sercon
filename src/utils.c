#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
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

int strnetcontent(char* s, char** start, char** end) {
	char *p1, *p2;
	if( ! s ) return 0;
	for(p1 = s; isspace(*p1); p1++)
		; // skip leading whitespace
	for(p2 = s + strlen(s) - 1; p2 >= p1 && isspace(*p2); p2--)
		; // move end pointer back over trailing whitespace
	if( start ) *start = p1;
	if( end ) *end = ++p2;
	return p2 - p1; // return length of the non-whitespace content
}
