#ifndef _GETOPT_EX_H
#define _GETOPT_EX_H

#include <getopt.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	struct option opt;
	const char* description;
	const char* argname;
} getopt_ex_option_t;

#define GETOPT_EX_OPTIONS_END   ((getopt_ex_option_t){ {0, 0, 0, 0}, 0, 0 })

// A simple wrapper around getopt_long that also generates help text and supports a callback for handling options
void getopt_ex(
	int argc,
	char* const *argv,
	const getopt_ex_option_t* long_options,
	const char* description,
	void(*callback)(int pos, int opt, const char* optarg)
);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _GETOPT_EX_H
