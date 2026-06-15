#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "command.h"

int parse_command_line(const char* line, int* argc, char*** argv) {
	typedef enum {
		PS_START,
		PS_IN_UNQUOTED,
		PS_IN_QUOTED,
		PS_ESCAPE,
		PS_END,
	} parse_state_t;

	parse_state_t state[32] = {0}; // state stack
	int statePos = 0; // current position in the state stack
	char quote = 0; // current quote character when in PS_IN_QUOTED state
	size_t tokenCount = 0; // number of tokens parsed so far
	char** tokens = 0; // array of token strings, will be realloc'd as we add tokens
	char token[256] = {0}; // buffer for the current token
	char* pTokenWr = token; // pointer to the current position in the token buffer

	state[statePos++] = PS_START;

	for(const char* p = line; p && *p ; p++) {
		if( pTokenWr - token >= (long)sizeof(token) - 1 ) {
			// FATAL: token buffer overflow
			fprintf(stderr, "Token buffer overflow!!\n");
			exit(1);
		}
		switch( state[statePos - 1] ) {
			case PS_START: {
				if( isspace(*p) ) break;
				// reset token buffer and start writing the first token
				pTokenWr = token;
				*pTokenWr = '\0';
				// select next state based on token type (quoted or unquoted)
				if( *p == '"' || *p == '\'' ) {
					quote = *p; // save the quote character to look for when we exit the quoted state
					state[statePos++] = PS_IN_QUOTED;
				} else {
					--p;
					state[statePos++] = PS_IN_UNQUOTED;
				}
				break;
			}
			case PS_IN_UNQUOTED: {
				if( isspace(*p) ) {
					--statePos; // end of this state, pop back to previous state
					--p; // reprocess this character in the new state
					state[statePos++] = PS_END;
				} else {
					*pTokenWr++ = *p;
					*pTokenWr = '\0';
				}
				break;
			}
			case PS_IN_QUOTED: {
				if( *p == '\\' && quote == '"' ) {
					state[statePos++] = PS_ESCAPE;
				} else if( *p == quote ) {
					--statePos;
					state[statePos++] = PS_END;
				} else {
					*pTokenWr++ = *p;
					*pTokenWr = '\0';
				}
				break;
			}
			case PS_ESCAPE: {
				switch( *p ) {
					case 'n': *pTokenWr++ = '\n'; break;
					case 'r': *pTokenWr++ = '\r'; break;
					case 't': *pTokenWr++ = '\t'; break;
					case 'b': *pTokenWr++ = '\b'; break;
					case 'a': *pTokenWr++ = '\a'; break;
					case 'f': *pTokenWr++ = '\f'; break;
					case '0': break; // ignore escaped null characters
					default: *pTokenWr++ = *p; break;
				}
				*pTokenWr = '\0';
				--statePos;
				break;
			}
			case PS_END: {
				// add this token to the argv array
				tokens = realloc(tokens, sizeof(char*) * (tokenCount + 1));
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
		tokens = realloc(tokens, sizeof(char*) * (tokenCount + 1));
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
