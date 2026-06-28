#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "command.h"
#include "r_array.h"
#include "utils.h"

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
#define PUSH(s) state[statePos++]=(s)
#define POP() --statePos
#define UNGET() --p
#define APPEND(c) *pTokenWr++ = (c); *pTokenWr = '\0'
#define RESET() pTokenWr = token; *pTokenWr = '\0'
	typedef enum {
		PS_START,
		PS_UNQUOTED,
		PS_QUOTED,
		PS_ESCAPE,
		PS_ESCAPE_OCTAL,
		PS_ESCAPE_HEX,
		PS_END,
	} parse_state_t;

	r_array_t atokens = r_array_create(100, free);
	parse_state_t state[32] = {0}; // state stack
	int statePos = 0; // current position in the state stack
	char quote = 0; // current quote character when in PS_QUOTED state
	char token[256] = {0}; // buffer for the current token
	char* pTokenWr = token; // pointer to the current position in the token buffer
	unsigned int integerValue = 0; // used for parsing octal and hex escape sequences
	int nExpectedDigits = 0; // number of expected digits for octal or hex escape sequences

	ASSERT(argc && argv);
	*argc = 0;
	*argv = 0;

	if( !line ) {
		return 0;
	}

	PUSH(PS_START);

	for(const char* p = line; *p ; p++) {
		// token buffer overflow check
		if( pTokenWr - token >= (long)sizeof(token) - 1 ) {
			// FATAL: token buffer overflow
			r_array_destroy(atokens);
			return -1;
		}
		// state stack overflow check
		ASSERT(statePos > 0 && statePos < (int)(sizeof(state)/sizeof(state[0])));
		switch( state[statePos - 1] ) {
			case PS_START: {
				if( isspace(*p) ) break;
				// reset token buffer
				RESET();
				// push token-completion state
				PUSH(PS_END);
				// select next state based on token type (quoted or unquoted)
				if( *p == '"' || *p == '\'' ) {
					// beginning of a quoted token, save the quote character and enter PS_QUOTED state
					quote = *p; // save the quote character to look for when we exit the quoted state
					PUSH(PS_QUOTED);
				} else {
					// beginning of an unquoted token, enter PS_UNQUOTED state
					UNGET();
					PUSH(PS_UNQUOTED);
				}
				break;
			}
			case PS_UNQUOTED: {
				if( isspace(*p) ) {
					// end of this token
					POP(); // end of this state, pop back to previous state
					UNGET(); // reprocess this character in the following state
					break;
				}
				APPEND(*p);
				break;
			}
			case PS_QUOTED: {
				if( *p == '\\' && quote == '"' ) {
					// push PS_ESCAPE state to process the next character
					PUSH(PS_ESCAPE);
				} else if( *p == quote ) {
					// end of this quoted token
					POP();
					quote = 0;
				} else {
					APPEND(*p);
				}
				break;
			}
			case PS_ESCAPE: {
				POP();
				switch( *p ) {
					case 'n': APPEND('\n'); break;
					case 'r': APPEND('\r'); break;
					case 't': APPEND('\t'); break;
					case 'b': APPEND('\b'); break;
					case 'a': APPEND('\a'); break;
					case 'f': APPEND('\f'); break;
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
						PUSH(PS_ESCAPE_OCTAL);
						UNGET(); // reprocess this character in the following state
						break;
					}
					case 'x': {
						// hex escape sequence, expect 2 hex digits
						integerValue = 0;
						nExpectedDigits = 2;
						PUSH(PS_ESCAPE_HEX);
						UNGET(); // reprocess this character in the following state
						break;
					}
					default: APPEND(*p); break;
				}
				break;
			}
			case PS_ESCAPE_OCTAL: {
				int done = 0;
				if( *p >= '0' && *p <= '7' ) {
					integerValue = (integerValue << 3) | (*p - '0');
				} else {
					UNGET(); // return this character to the previous state for processing
					done = 1;
				}
				if( --nExpectedDigits == 0 || done ) {
					ASSERT(integerValue <= 255);
					APPEND((char)(integerValue & 0xFF));
					POP(); // exit PS_ESCAPE_OCTAL state
				}
				break;
			}
			case PS_ESCAPE_HEX: {
				int stop = 0;
				if( isdigit(*p) ) {
					integerValue = (integerValue << 4) | (*p - '0');
				} else if( isxdigit(*p) ) {
					integerValue = (integerValue << 4) | (tolower(*p) - 'a' + 10);
				} else {
					// invalid hex digit
					UNGET(); // return this character to the previous state for processing
					stop = 1;
				}
				if( --nExpectedDigits == 0 || stop ) {
					ASSERT(integerValue <= 255);
					APPEND((char)(integerValue & 0xFF));
					POP(); // exit PS_ESCAPE_HEX state
				}
				break;
			}
			case PS_END: {
				r_array_add(atokens, strdup(token));
				// reset token buffer for the next token
				RESET();
				POP();
				UNGET(); // reprocess this character in the following state
				break;
			}
		} // end state machine switch
	} // end for

	// if there's still a pending token, commit it
	if( *token ) {
		r_array_add(atokens, strdup(token));
	}

	*argc = r_array_size(atokens);
	*argv = (char**)r_array_detach_elements(atokens);

	r_array_destroy(atokens);

	return *argc;
#undef PUSH
#undef POP
#undef UNGET
#undef APPEND
#undef RESET
}

/**
 * @brief Frees the memory allocated for command arguments.
 * @param argc The number of arguments.
 * @param argv The array of argument strings.
 */
void free_command_args(int argc, char** argv) {
	ASSERT(argv);
	for(int i = 0; i < argc; i++) {
		free(argv[i]);
	}
	free(argv);
}
