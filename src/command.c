#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "command.h"

#define ASSERT(x) assert(x)

/**
 * @brief Parses a command line into an argument vector, similar to how a shell would parse command line input.
 *
 * @param line The command line string to parse.
 * @param argc Pointer to an integer to store the number of arguments.
 * @param argv Pointer to an array of strings to store the arguments.
 * @return int The number of arguments parsed, or -1 on error (e.g., memory allocation failure or buffer overflow).
 * @note The caller is responsible for freeing the memory allocated for argv and its contents using free_command_args.
 */
int parse_command_line(const char* line, int* argc, char*** argv) {
	typedef enum {
		PS_START,
		PS_UNQUOTED,
		PS_QUOTED,
		PS_ESCAPE,
		PS_ESCAPE_OCTAL,
		PS_ESCAPE_HEX,
		PS_END,
	} parse_state_t;

	parse_state_t state[32] = {0}; // state stack
	int statePos = 0; // current position in the state stack
	char quote = 0; // current quote character when in PS_QUOTED state
	size_t tokenCount = 0; // number of tokens parsed so far
	char** tokens = 0; // array of token strings, will be realloc'd as we add tokens
	char token[256] = {0}; // buffer for the current token
	char* pTokenWr = token; // pointer to the current position in the token buffer
	unsigned int integerValue = 0; // used for parsing octal and hex escape sequences
	int nExpectedDigits = 0; // number of expected digits for octal or hex escape sequences

	ASSERT(argc && argv && line);
	*argc = 0;
	*argv = 0;

	state[statePos++] = PS_START;

	for(const char* p = line; p && *p ; p++) {
		// token buffer overflow check
		if( pTokenWr - token >= (long)sizeof(token) - 1 ) {
			// FATAL: token buffer overflow
			free_command_args(tokenCount, tokens);
			return -1;
		}
		// state stack overflow check
		ASSERT(statePos > 0 && statePos < (int)(sizeof(state)/sizeof(state[0])));
		switch( state[statePos - 1] ) {
			case PS_START: {
				if( isspace(*p) ) break;
				// reset token buffer and start writing the first token
				pTokenWr = token;
				*pTokenWr = '\0';
				// select next state based on token type (quoted or unquoted)
				if( *p == '"' || *p == '\'' ) {
					// beginning of a quoted token, save the quote character and enter PS_QUOTED state
					quote = *p; // save the quote character to look for when we exit the quoted state
					state[statePos++] = PS_QUOTED;
				} else {
					// beginning of an unquoted token, enter PS_UNQUOTED state
					--p; // pass this character to the PS_UNQUOTED state for processing
					state[statePos++] = PS_UNQUOTED;
				}
				break;
			}
			case PS_UNQUOTED: {
				if( isspace(*p) ) {
					// end of this token, push PS_END state to flush the token and prepare for the next one
					--statePos; // end of this state, pop back to previous state
					--p; // reprocess this character in the new state
					state[statePos++] = PS_END;
				} else {
					*pTokenWr++ = *p;
					*pTokenWr = '\0';
				}
				break;
			}
			case PS_QUOTED: {
				if( *p == '\\' && quote == '"' ) {
					// handle escaped characters only inside double quotes, push PS_ESCAPE state to process the next character
					state[statePos++] = PS_ESCAPE;
				} else if( *p == quote ) {
					// end of this quoted token, push PS_END state to flush the token and prepare for the next one
					--statePos;
					state[statePos++] = PS_END;
					quote = 0;
				} else {
					*pTokenWr++ = *p;
					*pTokenWr = '\0';
				}
				break;
			}
			case PS_ESCAPE: {
				--statePos;
				switch( *p ) {
					case 'n': *pTokenWr++ = '\n'; break;
					case 'r': *pTokenWr++ = '\r'; break;
					case 't': *pTokenWr++ = '\t'; break;
					case 'b': *pTokenWr++ = '\b'; break;
					case 'a': *pTokenWr++ = '\a'; break;
					case 'f': *pTokenWr++ = '\f'; break;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7': {
						// octal escape sequence, expect 3 octal digits maximum,
						// but can be less if a non-octal digit is encountered
						integerValue = 0;
						nExpectedDigits = 3;
						state[statePos++] = PS_ESCAPE_OCTAL;
						--p; // reprocess this character in the new state
						break;
					}
					case 'x': {
						// hex escape sequence, expect 2 hex digits
						integerValue = 0;
						nExpectedDigits = 2;
						state[statePos++] = PS_ESCAPE_HEX;
						break;
					}
					default: *pTokenWr++ = *p; break;
				}
				*pTokenWr = '\0';
				break;
			}
			case PS_ESCAPE_OCTAL: {
				if( *p >= '0' && *p <= '7' ) {
					integerValue = (integerValue << 3) | (*p - '0');
					if( --nExpectedDigits == 0 ) {
						ASSERT(integerValue <= 255);
						*pTokenWr++ = (char)integerValue;
						*pTokenWr = '\0';
						--statePos; // exit PS_ESCAPE_OCTAL state
					}
				} else {
					// invalid octal digit, terminate octal value and flush
					ASSERT(integerValue <= 255);
					*pTokenWr++ = (char)integerValue;
					*pTokenWr = '\0';
					--statePos;
				}
				break;
			}
			case PS_ESCAPE_HEX: {
				if( isxdigit(*p) ) {
					integerValue = (integerValue << 4) | (isdigit(*p) ? (*p - '0') : (tolower(*p) - 'a' + 10));
					if( --nExpectedDigits == 0 ) {
						ASSERT(integerValue <= 255);
						*pTokenWr++ = (char)integerValue;
						*pTokenWr = '\0';
						--statePos; // exit PS_ESCAPE_HEX state
					}
				} else {
					// invalid hex digit, treat as literal character and exit PS_ESCAPE_HEX state
					*pTokenWr++ = *p;
					*pTokenWr = '\0';
					--statePos;
				}
				break;
			}
			case PS_END: {
				// add this token to the argv array
				char** newTokens = realloc(tokens, sizeof(char*) * (tokenCount + 1));
				if( ! newTokens ) {
					// FATAL: memory allocation failure
					free_command_args(tokenCount, tokens);
					return -1;
				}
				tokens = newTokens;
				tokens[tokenCount++] = strdup(token);
				// reset token buffer for the next token
				pTokenWr = token;
				*pTokenWr = '\0';
				--statePos;
				--p; // reprocess this character in the new state
				break;
			}
		} // end state machine switch
	} // end for

	// if there's still a pending un-terminated token, flush it out as well
	if( *token ) {
		// add the last token if there is one after processing the whole line
		char** newTokens = realloc(tokens, sizeof(char*) * (tokenCount + 1));
		if( ! newTokens ) {
			// FATAL: memory allocation failure
			free_command_args(tokenCount, tokens);
			return -1;
		}
		tokens = newTokens;
		tokens[tokenCount++] = strdup(token);
	}

	*argc = tokenCount;
	*argv = tokens;

	return tokenCount;
}

void free_command_args(int argc, char** argv) {
	for(int i = 0; i < argc; i++) {
		free(argv[i]);
	}
	free(argv);
}
