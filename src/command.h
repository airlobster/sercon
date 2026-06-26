#ifndef _COMMAND_H
#define _COMMAND_H

/**
 * @file command.h
 * @brief bash-like Command-line parsing
 */

#ifdef __cplusplus
extern "C" {
#endif

int parse_command_line(const char* line, int* argc, char*** argv);
void free_command_args(int argc, char** argv);

#ifdef __cplusplus
}
#endif

#endif // _COMMAND_H
