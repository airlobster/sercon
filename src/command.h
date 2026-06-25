#ifndef _COMMAND_H
#define _COMMAND_H

/**
 * @file command.h
 * @brief bash-like Command-line parsing
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses a command line into an argument vector.
 * 
 * @param line The command line string to parse.
 * @param argc Pointer to an integer to store the argument count.
 * @param argv Pointer to an array of strings to store the arguments.
 * @return 0 on success, non-zero on failure.
 *
 * This function parses a command line into an argument vector, similar to how a shell would parse command line input.
 * It handles quoted strings (both single and double quotes) and escape sequences with backslash.
 * The caller is responsible for freeing the memory allocated for the argv array and its contents.
 */
int parse_command_line(const char* line, int* argc, char*** argv);
void free_command_args(int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif // _COMMAND_H
