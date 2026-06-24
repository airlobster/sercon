#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG_
#define DEBUG_MSG(fmt, ...) debug_msg(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
void debug_msg(const char* file, int line, const char* fmt, ...);
#define ASSERT(cond) if (!(cond)) { DEBUG_MSG("Assertion failed: '%s'", #cond); abort(); }
#else
#define DEBUG_MSG(fmt, ...) ((void)0)
#define ASSERT(cond) ((void)0)
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

int numdigits(long long v, unsigned short base);

int strnetcontent(char* s, char** start, char** end);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _UTILS_H_
