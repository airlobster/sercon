#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "ansi.h"
#include "utils.h"

#define ESC '\033'

ansi_mode_t ANSI_mode = ANSI_MODE_AUTO;
static bool bUseAltScreen = false;

/**
 * @brief Terminates ANSI mode and restores the terminal state.
 */
static void terminate_ansi() {
	DEBUG_MSG("Terminating ANSI mode");
	if( bUseAltScreen ) {
		ansi_fprintf(stdout, ANSI_END_ALT_SCREEN);
	}
	if( isAnsiActive(stdout) ) {
		ansi_fprintf(stdout, ANSI_RESET);
	}
	if( isAnsiActive(stderr) ) {
		ansi_fprintf(stderr, ANSI_RESET);
	}
}

/**
 * @brief Initializes ANSI mode and optionally enables the alternate screen buffer.
 * @param bAltScreen Whether to enable the alternate screen buffer.
 */
void begin_ansi(bool bAltScreen) {
	bUseAltScreen = bAltScreen;

	if( bUseAltScreen ) {
		DEBUG_MSG("Enabling alternate screen buffer");
		ansi_fprintf(stdout, ANSI_BEGIN_ALT_SCREEN ANSI_INIT_CURSOR_POS);
	}

	atexit(terminate_ansi);
}

/**
 * @brief Checks if ANSI mode is active for the given stream.
 * @param stream The file stream to check.
 * @return true If ANSI mode is active.
 * @return false If ANSI mode is not active.
 */
bool isAnsiActive(FILE* stream) {
	switch( ANSI_mode ) {
		case ANSI_MODE_AUTO:
			return isatty(fileno(stream));
		case ANSI_MODE_ALWAYS:
			return true;
		case ANSI_MODE_NEVER:
			return false;
	}
	return false; // should never reach here
}

/**
 * @brief Internal function for writing formatted output with ANSI codes.
 * @param stream The file stream to write to.
 * @param ref_stream The reference stream to check for tty mode.
 * @param fmt The format string.
 * @param args The variable argument list.
 * @return int The number of characters written.
 */
static int ansi_vwrite(FILE* stream, FILE* ref_stream, const char* fmt, va_list args) {
	const bool bAnsiActive = isAnsiActive(ref_stream);
	char *buffer;
	const char *pAnsiStart = 0;
	int n;

	// format the string with ANSI codes intact (they will be filtered out later if ANSI is not active)
	n = vasprintf(&buffer, fmt, args);

	// Process the formatted string character by character, handling ANSI escape sequences
	for(const char* p = buffer; *p; ++p) {
		if( pAnsiStart ) {
			// currently in an ANSI sequence
			if( isalpha(*p) ) {
				// end of ANSI escape sequence has been reached.
				// commit it to the output if ANSI is active, otherwise discard it
				if( bAnsiActive ) {
					// write out the entire last ANSI sequence
					while( pAnsiStart <= p ) {
						fputc(*pAnsiStart++, stream);
					}
				}
				pAnsiStart = 0; // reset to indicate we're no longer in an ANSI sequence
			}
		} else if( *p == ESC ) {
			// start of ANSI escape sequence
			pAnsiStart = p;
		} else {
			// normal character
			fputc(*p, stream);
		}
	} // end for

	// auto-reset any unclosed ANSI sequences at the end of the string
	if( bAnsiActive ) {
		fputs(ANSI_RESET, stream);
	}

	fflush(stream);
	free(buffer);

	return n;
}

/**
 * @brief Prints formatted output to a stream, while respecting ANSI color codes.
 * @param stream The file stream to print to.
 * @param fmt The format string.
 * @param args The variable argument list.
 * @return int The number of characters printed.
 * @note ANSI color codes will only be printed if ANSI mode is active for the stream.
 */
int ansi_vfprintf(FILE* stream, const char* fmt, va_list args) {
	return ansi_vwrite(stream, stream, fmt, args);
}

/**
 * @brief Prints formatted output to a stream, while respecting ANSI color codes.
 * @param stream The file stream to print to.
 * @param fmt The format string.
 * @param ... The variable arguments.
 * @return int The number of characters printed.
 * @note ANSI color codes will only be printed if ANSI mode is active for the stream.
 */
int ansi_fprintf(FILE* stream, const char* fmt, ...) {
	va_list args;
	int n;

	va_start(args, fmt);
	n = ansi_vwrite(stream, stream, fmt, args);
	va_end(args);

	return n;
}

int ansi_asprintf(char** out, const char* fmt, ...) {
	va_list args;
	int n;
	char* buffer = 0;
	size_t size = 0;

	FILE* stream = open_memstream(&buffer, &size);
	if( ! stream ) {
		return -1;
	}

	va_start(args, fmt);
	n = ansi_vwrite(stream, stdout, fmt, args);
	va_end(args);
	fflush(stream);
	fclose(stream);

	*out = buffer;

	return n;
}

/**
 * @brief Prints an informational message with ANSI styling.
 * @param fmt The format string.
 * @param ... The variable arguments.
 * @return int The number of characters printed.
 */
int a_info(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_INFO "%s", fmt);

	va_start(args, fmt);
	n = ansi_vfprintf(stdout, buf, args);
	va_end(args);

	free(buf);

	return n;
}

/**
 * @brief Prints an error message with ANSI styling.
 * @param fmt The format string.
 * @param ... The variable arguments.
 * @return int The number of characters printed.
 */
int a_error(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_ERROR "%s", fmt);

	va_start(args, fmt);
	n = ansi_vfprintf(stderr, buf, args);
	va_end(args);

	free(buf);

	return n;
}

/**
 * @brief Prints a warning message with ANSI styling.
 * @param fmt The format string.
 * @param ... The variable arguments.
 * @return int The number of characters printed.
 */
int a_warning(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_WARNING "%s", fmt);

	va_start(args, fmt);
	n = ansi_vfprintf(stderr, buf, args);
	va_end(args);

	free(buf);

	return n;
}

/**
 * @brief Prints a success message with ANSI styling.
 * @param fmt The format string.
 * @param ... The variable arguments.
 * @return int The number of characters printed.
 */
int a_success(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_SUCCESS "%s", fmt);

	va_start(args, fmt);
	n = ansi_vfprintf(stdout, buf, args);
	va_end(args);

	free(buf);

	return n;
}

/**
 * @brief Sets the ANSI mode.
 * @param mode The ANSI mode ("auto", "always", "never").
 * @return true if the mode was set successfully, false otherwise.
 */
bool set_ansi_mode(const char* mode) {
	if( strcasecmp(mode, "auto") == 0 ) {
		ANSI_mode = ANSI_MODE_AUTO;
	} else if( strcasecmp(mode, "always") == 0 ) {
		ANSI_mode = ANSI_MODE_ALWAYS;
	} else if( strcasecmp(mode, "never") == 0 ) {
		ANSI_mode = ANSI_MODE_NEVER;
	} else {
		return false;
	}
	return true;
}
