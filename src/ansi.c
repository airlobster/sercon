#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "ansi.h"

#define ESC '\033'

ansi_mode_t ANSI_mode = ANSI_MODE_AUTO;
static bool bUseAltScreen = false;

static void terminate_ansi() {
	if( bUseAltScreen ) {
		afprintf(stdout, ANSI_END_ALT_SCREEN);
	}

	if( isAnsiActive(stdout) ) {
		afprintf(stdout, ANSI_RESET);
	}
	if( isAnsiActive(stderr) ) {
		afprintf(stderr, ANSI_RESET);
	}
}

void begin_ansi(bool bAltScreen) {
	// if( ! isatty(fileno(stdout)) || ! isatty(fileno(stderr)) ) {
	//   a_error("STDOUT and STDERR are not allowed to be redirected!\n");
	//   exit(1);
	// }

	bUseAltScreen = bAltScreen;

	if( bUseAltScreen ) {
		afprintf(stdout, ANSI_BEGIN_ALT_SCREEN ANSI_INIT_CURSOR_POS);
	}

	atexit(terminate_ansi);
}

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

// Custom printf function that respects ANSI color codes
// (i.e. only prints them if the stream is a TTY or if ANSI_MODE_ALWAYS is set)
int afvprintf(FILE* stream, const char* fmt, va_list args) {
	const bool bAnsiActive = isAnsiActive(stream);
	char *buffer;
	const char *pAnsiStart = 0;
	char *codesStack[256] = {0};
	int codesStackPos = 0;
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
					if( *p == 'Z' ) {
						// custom sequence for popping last sequence from stack to un-apply it.
						// (used for nested styles like "red bold red" where the second "red"
						// should only reset the "bold" but not the first "red")
						if( codesStackPos > 0 ) {
							free(codesStack[--codesStackPos]);
							// re-render any remaining active ANSI codes to ensure the correct styles are still applied after the reset
							fputs(ANSI_RESET, stream);
							for(int i = 0; i < codesStackPos; ++i ) {
								fputs(codesStack[i], stream);
							}
						}
					} else {
						// push the ANSI code onto a stack so that we can auto-reset it at the end of the string if needed
						codesStack[codesStackPos++] = strndup(pAnsiStart, p - pAnsiStart + 1);
						// write out the entire last ANSI sequence
						while( pAnsiStart <= p ) {
							fputc(*pAnsiStart++, stream);
						}
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

	// cleanup
	for( int i = 0; i < codesStackPos; ++i ) {
		free(codesStack[i]);
	}
	free(buffer);

	return n;
}

// Simple wrapper around afvprintf for convenience
int afprintf(FILE* stream, const char* fmt, ...) {
	va_list args;
	int n;

	va_start(args, fmt);
	n = afvprintf(stream, fmt, args);
	va_end(args);

	return n;
}

int a_info(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_INFO "%s", fmt);

	va_start(args, fmt);
	n = afvprintf(stdout, buf, args);
	va_end(args);

	free(buf);

	return n;
}

int a_error(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_ERROR "%s", fmt);

	va_start(args, fmt);
	n = afvprintf(stderr, buf, args);
	va_end(args);

	free(buf);

	return n;
}

int a_warning(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_WARNING "%s", fmt);

	va_start(args, fmt);
	n = afvprintf(stderr, buf, args);
	va_end(args);

	free(buf);

	return n;
}

int a_success(const char* fmt, ...) {
	char* buf;
	va_list args;
	int n;

	asprintf(&buf, ANSI_SUCCESS "%s", fmt);

	va_start(args, fmt);
	n = afvprintf(stdout, buf, args);
	va_end(args);

	free(buf);

	return n;
}

