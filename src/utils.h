#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define autoptr(f) __attribute__((cleanup(f)))
#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))

const char* getHomeDir();

typedef struct {
	int day;
	int month;
	int year;
	int hours;
	int minutes;
	int seconds;
	int milliseconds;
} cal_time_t;
int now(cal_time_t* t);

// return boundaries of a string's net content (minus leading and trailing whitespace)
int strnetcontent(char* s, char** start, char** end);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _UTILS_H_
