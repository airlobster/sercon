#ifndef _SHELL_H
#define _SHELL_H

/**
 * @brief Execute a shell command.
 * @param argv The argument vector for the command.
 * @param input Optional input to pass to the command's stdin.
 * @return int The exit status of the command.
 */

#ifdef __cplusplus
extern "C" {
#endif

int sc_shell(const char* argv[], const char* input);

#ifdef __cplusplus
}
#endif

#endif // _SHELL_H
