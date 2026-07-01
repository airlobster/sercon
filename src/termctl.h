#ifndef _TERMCTL_H
#define _TERMCTL_H

/**
 * @file termctl.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-06-30
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "readline_ex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Result codes for termctl operations.
 */
typedef enum {
	TERMCTL_R_OK = 0,
	TERMCTL_R_POLLERROR = 1,
} termctl_result_t;

/**
 * @brief handle for termctl.
 */
typedef struct _termctl_internal_t* termctl_t;

/**
 * @brief Callback function type for updating the prompt.
 *
 * @param tc The termctl instance.
 * @param userData User data pointer.
 * @return char* The new prompt string.
 */
typedef char* (*termctl_prompt_callback_t)(termctl_t tc, void* userData);

/**
 * @brief Callback function type for handling newlines.
 *
 * @param tc The termctl instance.
 * @param userData User data pointer.
 */
typedef void (*termctl_newline_callback_t)(termctl_t tc, void* userData);

/**
 * @brief Callback function type for handling user input.
 *
 * @param tc The termctl instance.
 * @param line The input line.
 * @param length The length of the input line.
 * @param userData User data pointer.
 */
typedef void (*termctl_user_input_callback_t)(termctl_t tc, const char* line, size_t length, void* userData);

/**
 * @brief Callback function type for handling reconnection attempts.
 *
 * @param tc The termctl instance.
 * @param fd The file descriptor to reconnect.
 * @param userData User data pointer.
 * @return fd if successful, -1 if failed.
 */
typedef int (*termctl_reconnect_callback_t)(termctl_t tc, int fd, void* userData);

termctl_t termctl_create(const char* appname, void* userData);
void termctl_destroy(termctl_t termctl);

void termctl_set_prompt_callback(termctl_t termctl, termctl_prompt_callback_t callback);
void termctl_set_newline_callback(termctl_t termctl, termctl_newline_callback_t callback);
void termctl_set_user_input_callback(termctl_t termctl, termctl_user_input_callback_t callback);
void termctl_set_reconnect_callback(termctl_t termctl, termctl_reconnect_callback_t callback);

int termctl_add_fd(termctl_t termctl, int fd);
int termctl_remove_fd(termctl_t termctl, int fd);

rlx_t termctl_get_rlx(termctl_t termctl);

termctl_result_t termctl_event_loop(termctl_t termctl);

#ifdef __cplusplus
}
#endif

#endif // _TERMCTL_H
