#ifndef _SHELL_H
#define _SHELL_H

/**
 * @file shell.h
 * @brief Shell command execution interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

int sc_shell(const char* argv[], const char* input);

#ifdef __cplusplus
}
#endif

#endif // _SHELL_H
