#ifndef _ANSI_H_
#define _ANSI_H_

/**
 * @file ansi.h
 * @brief Header file for ANSI escape codes and related functions.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

#define ANSI_CSI                "\033["

/**
 * @brief ANSI escape codes.
 */

#define ANSI_RESET              ANSI_CSI "0m"
#define ANSI_BOLD               ANSI_CSI "1m"
#define ANSI_DIM                ANSI_CSI "2m"
#define ANSI_ITALIC             ANSI_CSI "3m"
#define ANSI_UNDERLINE          ANSI_CSI "4m"

#define ANSI_RED                ANSI_CSI "31m"
#define ANSI_GREEN              ANSI_CSI "32m"
#define ANSI_YELLOW             ANSI_CSI "33m"
#define ANSI_BLUE               ANSI_CSI "34m"
#define ANSI_MAGENTA            ANSI_CSI "35m"
#define ANSI_CYAN               ANSI_CSI "36m"
#define ANSI_WHITE              ANSI_CSI "37m"

#define ANSI_B_RED              ANSI_CSI "91m"
#define ANSI_B_GREEN            ANSI_CSI "92m"
#define ANSI_B_YELLOW           ANSI_CSI "93m"
#define ANSI_B_BLUE             ANSI_CSI "94m"
#define ANSI_B_MAGENTA          ANSI_CSI "95m"
#define ANSI_B_CYAN             ANSI_CSI "96m"
#define ANSI_B_WHITE            ANSI_CSI "97m"

#define ANSI_INIT_CURSOR_POS    ANSI_CSI "H"
#define ANSI_SAVE_CURSOR        ANSI_CSI "s"
#define ANSI_RESTORE_CURSOR     ANSI_CSI "u"
#define ANSI_CLEAR_LINE         ANSI_CSI "K"
#define ANSI_BEGIN_ALT_SCREEN   ANSI_CSI "?1049h"
#define ANSI_END_ALT_SCREEN     ANSI_CSI "?1049l"

// Convenience macros for common styles
#define ANSI_INFO               ANSI_DIM ANSI_ITALIC
#define ANSI_ERROR              ANSI_RED
#define ANSI_SUCCESS            ANSI_GREEN
#define ANSI_WARNING            ANSI_YELLOW

#define IS_ANSI_END_CHAR(c) 		((c) >= 0x40 && (c) <= 0x7E && (c) != '[')
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ANSI mode enumeration.
 * @details This enumeration defines the different modes for ANSI escape code handling.
 */
typedef enum {
	ANSI_MODE_AUTO,
	ANSI_MODE_ALWAYS,
	ANSI_MODE_NEVER
} ansi_mode_t;

extern ansi_mode_t ANSI_mode;

void begin_ansi(bool bAltScreen);
bool isAnsiActive(FILE* stream);
bool set_ansi_mode(const char* mode);
const char* get_ansi_mode(void);

// Print formatted output to a stream, while respecting ANSI color codes
// (i.e. only print them if the stream is a TTY or if forceANSI is true)
int ansi_fprintf(FILE* stream, const char* fmt, ...);
int ansi_vfprintf(FILE* stream, const char* fmt, va_list args);
int ansi_asprintf(char** out, const char* fmt, ...);

// helpers
int a_info(const char* fmt, ...);
int a_error(const char* fmt, ...);
int a_warning(const char* fmt, ...);
int a_success(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* _ANSI_H_ */
