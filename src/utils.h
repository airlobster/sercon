#ifndef _UTILS_H_
#define _UTILS_H_

/**
 * @file utils.h
 * @brief Utility macros and functions
 *
 */

#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _DEBUG_
/**
 * @brief Print a debug message with file and line information.
 * @param fmt The format string.
 * @param ... The format arguments.
 */
#define DEBUG_MSG(fmt, ...) debug_msg(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
void debug_msg(const char* file, int line, const char* fmt, ...);
/**
 * @brief Assert a condition and print a debug message if the assertion fails.
 * @param cond The condition to assert.
 */
#define ASSERT(cond) if (!(cond)) { DEBUG_MSG("Assertion failed: '%s'", #cond); exit(-1); }
#else
#define DEBUG_MSG(fmt, ...) ((void)0)
#define ASSERT(cond) ((void)0)
#endif

/**
 * @brief Macro to automatically call a cleanup function when a variable goes out of scope.
 * @param f The cleanup function to call.
 */
#define autoptr(f) __attribute__((cleanup(f)))

/**
 * @brief Macro to calculate the number of elements in an array.
 * @param arr The array.
 * @return The number of elements in the array.
 */
#define array_size(arr) (sizeof(arr) / sizeof((arr)[0]))

/**
 * @brief Macro to define a constructor function that runs before main().
 * @param decl The function signature.
 */
#define INITIALIZER(decl) decl __attribute__((constructor)); decl

const char* getHomeDir();

/**
 * @brief Structure representing a calendar time.
 * @details This structure holds information about the day, month, year, hours, minutes, seconds, and milliseconds.
 */
typedef struct {
	int day; /**< The day of the month. */
	int month; /**< The month of the year. */
	int year; /**< The year. */
	int hours; /**< The hour of the day. */
	int minutes; /**< The minute of the hour. */
	int seconds; /**< The second of the minute. */
	int milliseconds; /**< The millisecond of the second. */
} cal_time_t;
int now(cal_time_t* t);

int numdigits(long long v, unsigned short base);

int strnetcontent(char* s, char** start, char** end);
int parse_path_list(const char* pathlist, int* argc, char*** argv);

typedef enum {
	CGLOB_FILE_REGULAR = 1UL << 0, /**< Regular file */
	CGLOB_FILE_DIRECTORY = 1UL << 1, /**< Directory */
	CGLOB_FILE_SYMLINK = 1UL << 2, /**< Symbolic link */
	CGLOB_FILE_CHAR_DEVICE = 1UL << 3, /**< Character device */
	CGLOB_FILE_BLOCK_DEVICE = 1UL << 4, /**< Block device */
	CGLOB_FILE_FIFO = 1UL << 5, /**< FIFO (named pipe) */
	CGLOB_FILE_SOCKET = 1UL << 6, /**< Socket */
} cglob_file_type_t;
typedef void(*cglob_callback_t)(const char* path, void* userData);
int cglob(const char* pattern, unsigned long options, cglob_callback_t callback, void* userData);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // _UTILS_H_
