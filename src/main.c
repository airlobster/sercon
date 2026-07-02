#include <stdio.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include "utils.h"
#include "getopt_ex.h"
#include "serlist.h"
#include "ansi.h"
#include "termctl.h"
#include "settings.h"
#include "shell.h"

#ifndef VERSION
#define VERSION "0.0.0.0"
#endif

int baud = 9600;
bool printTimestamps = true;
char* port = 0;
int fdPort = -1;
termctl_t termctl = 0;
settings_t settings = 0;

/**
 * @brief Callback function for shell command enumeration.
 * @param command The shell command.
 * @param context User data pointer.
 */
static void add_words_to_vocabulary_callback(const char* word, void* context) {
	termctl_t tc = (termctl_t)context;
	ASSERT(tc);
	rlx_add_autocomplete_vocabulary_entry(termctl_get_rlx(tc), word);
}

/**
 * @brief Print a serial port name.
 * @param portName The name of the serial port.
 * @param context User data pointer.
 */
static void enum_ports_print_callback(const char* portName, void* context) {
	(void)context;
	ansi_fprintf(stdout, ANSI_ITALIC "  %s\n", portName);
}

void shell_stdout_callback(const char* output, size_t length, void* context) {
	(void)context;
	(void)length;
	ASSERT(context);
	a_normal("%s", output);
}

void shell_stderr_callback(const char* output, size_t length, void* context) {
	(void)context;
	(void)length;
	ASSERT(context);
	a_error("%s", output);
}

/**
 * @brief Print the list of available serial ports.
 */
static void print_ports_list() {
	ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available serial ports:\n");
	int n = enumSerialPorts(enum_ports_print_callback, 0);
	if( ! n ) {
		ansi_fprintf(stdout, ANSI_ITALIC "  No serial ports found\n");
	}
}

/**
 * @brief Connect to a serial port.
 * @param tc The termctl instance.
 * @param portName The name of the serial port.
 * @param baudRate The baud rate for the connection.
 * @return int The file descriptor of the opened port, or -1 on failure.
 */
static int connect(termctl_t tc, const char* portName, int baudRate) {
	(void)tc;
	ASSERT(portName);
	ASSERT(baudRate > 0);
	int fd = open(portName, O_RDWR | O_NOCTTY);
	if( fd < 0 ) {
		return -1;
	}
	// set baud rate
	struct termios t;
	tcgetattr(fd, &t);
	cfsetspeed(&t, baudRate);
	tcsetattr(fd, TCSANOW, &t);
	return fd;
}

/**
 * @brief Disconnect from the current serial port.
 * @param tc The termctl instance.
 */
static bool disconnect(termctl_t tc) {
	(void)tc;
	if( port ) {
		free(port);
		port = 0;
	}
	if( fdPort > 0 ) {
		close(fdPort);
		fdPort = -1;
		return true;
	}
	return false;
}

/**
 * @brief Run a script file.
 * @param tc The termctl instance.
 * @param scriptPath The path to the script file.
 * @return bool True if the script was run successfully, false otherwise.
 */
bool run_script(termctl_t tc, const char* scriptPath) {
	ASSERT(tc);
	ASSERT(scriptPath);
	FILE* scriptFile = fopen(scriptPath, "r");
	if( ! scriptFile ) {
		a_error("Failed to open script file: %s\n", scriptPath);
		return false;
	}
	for(;;) {
		char line[1024];
		if( ! fgets(line, sizeof(line), scriptFile) ) break;
		termctl_inject_input(tc, line);
	}
	fclose(scriptFile);
	return true;
}

/**
 * @brief Apply a connection string to connect to a serial port.
 * @param tc The termctl instance.
 * @param connectionString The connection string in the format "PORT{:BAUD}".
 * @return int The file descriptor of the opened port, or -1 on failure.
 */
static int applyConnectionString(termctl_t tc, const char *connectionString) {
	char portName[256];
	int baudRate = 9600;
	ASSERT(connectionString);
	int n = sscanf(connectionString, "%[^:]:%d", portName, &baudRate);
	if( n < 1 ) {
		a_error("Invalid port specification: %s\n", connectionString);
		return -1;
	}
	return connect(tc, portName, MAX(baudRate, 9600));
}

/**
 * @brief CLI argument callback function.
 * @param tc The termctl instance.
 */
static void cli_args_callback(int pos, int opt, const char* optarg) {
	(void)pos;
	switch( opt ) {
		case 'l': {
			print_ports_list();
			exit(0);
		}
		case 'p': {
			ASSERT(! port);
			port = strdup(optarg);
			break;
		}
		case 'T': {
			printTimestamps = false;
			break;
		}
		case 'v': {
			ansi_fprintf(stdout, "v%s\n", VERSION);
			exit(0);
		}
		case 'c': {
			if( ! set_ansi_mode(optarg) ) {
				a_error("Invalid color mode: '%s'\n", optarg);
				exit(1);
			}
			break;
		}
		case 0: {
			a_error("Unexpected positional argument: %s\n", optarg);
			exit(1);
		}
	} // end switch
}

/**
 * @brief Parse command-line arguments.
 * @param argc The argument count.
 * @param argv The argument vector.
 */
static void parse_cli_args(int argc, char *argv[])
{
	static const char* description = "sercon - A simple serial console for Arduino development\n\n"
		"This tool allows you to connect to a serial port and interact with it in real-time.\n"
		"You can specify the port and baud rate, and it will display incoming data with optional\n"
		"timestamps. Use the --list option to see available serial ports on your system.";
	static const getopt_ex_option_t options[] = {
		{{"list", no_argument, 0, 'l'}, "List available serial ports", 0},
		{{"port", required_argument, 0, 'p'}, "Specify the serial port", "PORT{:BAUD}"},
		{{"no-timestamps", no_argument, 0, 'T'}, "Disable timestamps", 0},
		{{"version", no_argument, 0, 'v'}, "Show version information", 0},
		{{"color", required_argument, 0, 'c'}, "Set ANSI color mode (auto|always|never)", "MODE"},
		GETOPT_EX_OPTIONS_END
	};
	ASSERT(options[array_size(options)-1].opt.name == 0); // ensure the last option is the end marker
	getopt_ex(argc, argv, options, description, cli_args_callback);
}

/**
 * @brief Callback function for registered commands.
 * @param h The rlx handle.
 * @param cmd The registered command.
 * @param argc The argument count.
 * @param argv The argument vector.
 * @param context User data pointer.
 */
static void registered_commands_callback(
		rlx_t h, const rlx_registered_command_t* cmd, int argc, const char* argv[], void* context) {
	termctl_t tc = (termctl_t)context;
	ASSERT(h);
	switch( cmd->id ) {
		case 'h': {
			if( argc == 1 ) {
				ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available commands:\n");
				rlx_print_registered_commands(h);
			} else {
				for(int i=1; i<argc; ++i) {
					const rlx_registered_command_t* c = rlx_get_command(h, argv[i]);
					if( c ) {
						ansi_fprintf(stdout, "%-10s - %s\n", c->command, c->description);
					} else {
						a_error("Unknown command: %s\n", argv[i]);
					}
				}
			}
			break;
		}
		case 'p': {
			print_ports_list();
			break;
		}
		case 'c': {
			rlx_reset_history(h);
			a_success("History cleared\n");
			break;
		}
		case 'q': {
			exit(0);
			break;
		}
		case 'v': {
			ansi_fprintf(stdout, "v%s\n", VERSION);
			break;
		}
		case 'i': {
			rlx_print_history(h);
			break;
		}
		case 'C': { // connect
			if( argc < 2 ) {
				a_error("Usage: connect PORT{:BAUD}\n");
				break;
			}
			if( fdPort > 0 ) {
				a_error("Already connected. Please disconnect first.\n");
				break;
			}
			fdPort = applyConnectionString(tc, argv[1]);
			if( fdPort > 0 ) {
				port = strdup(argv[1]);
				termctl_add_fd(tc, fdPort);
			}
			break;
		}
		case 'D': { // disconnect
			if( fdPort < 0 ) {
				a_error("Not currently connected to any port\n");
				break;
			}
			int fd = fdPort;
			if( disconnect(tc) ) {
				termctl_remove_fd(tc, fd);
			}
			break;
		}
		case 't': {
			if( argc > 1 ) {
				printTimestamps = strcasecmp(argv[1], "on") == 0 || strcasecmp(argv[1], "yes") == 0;
			}
			settings_set(settings, "timestamps", printTimestamps ? "on" : "off");
			a_success("Timestamps %s\n", printTimestamps ? "on" : "off");
			break;
		}
		case 'R': {
			if( argc > 1 ) {
				if( ! set_ansi_mode(argv[1]) ) {
					a_error("Invalid color mode: '%s'\n", argv[1]);
					break;
				}
			}
			settings_set(settings, "color_mode", get_ansi_mode());
			a_success("Color mode: %s\n", get_ansi_mode());
			break;
		}
		case 's': {
			if( argc < 2 ) {
				a_error("Usage: script PATH\n");
				break;
			}
			run_script(tc, argv[1]);
			break;
		}
		case 'B': {
			if( argc < 2 ) {
				a_error("Usage: shell COMMAND\n");
				break;
			}
			ASSERT(argv[argc] == NULL); // ensure argv is null-terminated
			int err = sc_shell_v(argv + 1, 0, shell_stdout_callback, shell_stderr_callback, tc);
			if( err ) {
				a_error("Shell command failed with exit code %d\n", err);
			}
			break;
		}
#ifdef _DEBUG_
		case 'A': {
			rlx_print_autocomplete_vocabulary(h);
			break;
		}
		case 'S': {
			settings_print(settings);
			break;
		}
#endif
	}
}

/**
 * @brief Setup the registered commands for the terminal.
 * @param termctl The termctl instance.
 */
static void setupTerminalRegisteredCommands(termctl_t termctl) {
	static const rlx_registered_command_t commands[] = {
		{'i', "history", "Show command history", registered_commands_callback},
		{'c', "clear", "Clear history", registered_commands_callback},
		{'h', "help", "Show this help message", registered_commands_callback},
		{'q', "quit", "Exit the program", registered_commands_callback},
		{'v', "version", "Show version information", registered_commands_callback},
		{'p', "ports", "List available serial ports", registered_commands_callback},
		{'C', "connect", "Connect to a serial port (usage: connect PORT{:BAUD})", registered_commands_callback},
		{'D', "disconnect", "Disconnect from the current serial port", registered_commands_callback},
		{'t', "timestamps", "Set/show timestamps state (on|off)", registered_commands_callback},
		{'R', "colors", "Set/show ANSI color mode (auto|always|never)", registered_commands_callback},
		{'s', "script", "Run a script file (usage: script PATH)", registered_commands_callback},
		{'B', "shell", "Run a shell command (usage: shell COMMAND)", registered_commands_callback},
#ifdef _DEBUG_
		{'A', "vocabulary", "Show auto-complete vocabulary (debugging only)", registered_commands_callback},
		{'S', "settings", "Show current settings (debugging only)", registered_commands_callback},
#endif
		{0, 0, 0, 0} // end marker
	};
	ASSERT(termctl);
	ASSERT(commands[array_size(commands)-1].command == 0); // ensure the last command is the end marker
	rlx_register_commands(termctl_get_rlx(termctl), commands);
}

/**
 * @brief Prompt callback function for a termctl instance.
 * @param tc The termctl instance.
 * @param context User data pointer.
 * @return char* The prompt string.
 * @details termctl will free the returned string after use, so it should be dynamically allocated.
 */
static char* prompt_callback(termctl_t tc, void* context) {
	ASSERT(tc);
	if( ! isatty(fileno(stdin)) ) return 0; // no prompt if stdin is redirected!!
	char *p = 0;
	if( fdPort >= 0 ) {
		// connected
		ASSERT(port);
		ansi_asprintf(&p, ANSI_SUCCESS ANSI_ITALIC "%s> ", port);
	} else if( port ) {
		// connection lost
		ansi_asprintf(&p, ANSI_ERROR ANSI_ITALIC "%s...> ", port);
	} else {
		// disconnected
		ansi_asprintf(&p, ANSI_INFO ANSI_ITALIC "%s> ", "not-connected");
	}
	return p;
}

/**
 * @brief User input callback function for a termctl instance.
 * @param tc The termctl instance.
 * @param line The input line.
 * @param length The length of the input line.
 * @param context User data pointer.
 */
void user_input_callback(termctl_t tc, const char* line, size_t length, void* context) {
	if( fdPort > 0 ) {
		write(fdPort, line, length);
		write(fdPort, "\n", 1);
	} else {
		a_error("Not connected to any serial port. Use 'connect <PORT>' to connect.\n");
	}
}

/**
 * @brief Newline callback function for a termctl instance.
 * @param tc The termctl instance.
 * @param context User data pointer.
 */
void newline_callback(termctl_t tc, void* context) {
	(void)tc;
	if( ! printTimestamps ) return;
	cal_time_t t;
	now(&t);
	ansi_fprintf(stdout, ANSI_INFO "[%02d:%02d:%02d.%03d] ",
		t.hours, t.minutes, t.seconds, t.milliseconds);
}

/**
 * @brief Reconnect callback function for a termctl instance.
 * @param tc The termctl instance.
 * @param context User data pointer.
 * @return fd if successful, -1 if failed.
 */
int reconnect_callback(termctl_t tc, int fd, void* context) {
	(void)context;
	(void)fd;
	ASSERT(tc);
	ASSERT(port);
	return fdPort = applyConnectionString(tc, port);
}

/**
 * @brief Autocomplete callback function for a termctl instance.
 * @param rlx The rlx instance.
 * @param context User data pointer.
 */
static void autocomplete_callback(rlx_t rlx, void* context) {
	(void)context;
	ASSERT(rlx);
	// add ports to the autocomplete vocabulary
	enumSerialPorts(add_words_to_vocabulary_callback, termctl);
}

/**
 * @brief Exit handler for the application.
 */
static void on_exit_handler(void) {
	DEBUG_MSG("Shutting down application");
	if( termctl ) {
		termctl_destroy(termctl);
		termctl = 0;
	}
	if( fdPort > 0 ) {
		close(fdPort);
		fdPort = -1;
	}
	if( port ) {
		free(port);
		port = 0;
	}
	if( settings ) {
		settings_free(settings);
		settings = 0;
	}
}

static void apply_loaded_settings() {
	ASSERT(settings);
	const char* color_mode = settings_get(settings, "color_mode");
	if( color_mode ) {
		DEBUG_MSG("Loaded color mode from settings: %s", color_mode);
		set_ansi_mode(color_mode);
	}
	const char* timestamps = settings_get(settings, "timestamps");
	if( timestamps ) {
		DEBUG_MSG("Loaded timestamps setting from settings: %s", timestamps);
		printTimestamps = strcasecmp(timestamps, "on") == 0 || strcasecmp(timestamps, "yes") == 0;
	}
}

int main(int argc, char* argv[]) {
	const char* appname = basename(argv[0]);

	settings = settings_init(appname);
	atexit(on_exit_handler);

	parse_cli_args(argc, argv);
	begin_ansi(false);
	apply_loaded_settings();

	// print banner (only if stdin is a terminal)
	if( isatty(fileno(stdin)) ) {
		ansi_fprintf(stdout, ANSI_BOLD "%s - A Serial-Ports Console (v%s)\n", appname, VERSION);
	}

	termctl = termctl_create(appname, 0);
	if( ! termctl ) {
		a_error("Failed to create termctl instance.\n");
		return 1;
	}

	// set callbacks for termctl and its underlying rlx instance
	termctl_set_prompt_callback(termctl, prompt_callback);
	termctl_set_user_input_callback(termctl, user_input_callback);
	termctl_set_newline_callback(termctl, newline_callback);
	termctl_set_reconnect_callback(termctl, reconnect_callback);
	rlx_add_autocomplete_callback(termctl_get_rlx(termctl), autocomplete_callback);

	setupTerminalRegisteredCommands(termctl);

	// if port was specified in the command-line, attempt to connect to it
	// before entering the event loop
	if( port ) {
		ASSERT(fdPort < 0); // should only call connect() when not currently connected
		if( (fdPort = applyConnectionString(termctl, port)) > 0 ) {
			termctl_add_fd(termctl, fdPort);
		} else {
			a_error("Failed to connect to %s\n", port);
		}
	}

	return termctl_event_loop(termctl);
}
