#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT(cond) assert(cond)

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

int numdigits(long long v, unsigned short base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _UTILS_H_
