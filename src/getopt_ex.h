#ifndef _GETOPT_EX_H
#define _GETOPT_EX_H

/**
 * @file getopt_ex.h
 * @author your name (you@domain.com)
 * @brief Extended getopt functionality with help text and callbacks
 * @version 0.1
 * @date 2026-06-16
 */
#include <getopt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Extended option structure for getopt_ex
 * @details This structure extends the standard `struct option` with additional fields for description and argument name.
 */
typedef struct {
	struct option opt; /**< The standard `struct option` from getopt.h. */
	const char* description; /**< A string describing the option, used for generating help text. */
	const char* argname; /**< A string representing the name of the argument for this option, used in help text. */	
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
