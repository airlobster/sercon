#ifndef _SHELL_H
#define _SHELL_H

/**
 * @file shell.h
 * @brief Shell command execution interface.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*shell_output_callback_t)(const char* output, size_t length, void* context);

int sc_shell_v(
	const char* argv[],
	const char* input,
	shell_output_callback_t stdout_callback,
	shell_output_callback_t stderr_callback,
	void* context
);

int sc_shell(
	const char* command,
	const char* input,
	shell_output_callback_t stdout_callback,
	shell_output_callback_t stderr_callback,
	void* context
);

char* sc_shell_make_command(const char* shell, int argc, const char* argv[]);

#ifdef __cplusplus
}
#endif

#endif // _SHELL_H
