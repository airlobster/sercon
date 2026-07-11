#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/param.h>
#include <termios.h>
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

static bool printTimestamps = true;
static char* port = 0;
static int fdPort = -1;
static termctl_t termctl = 0;
static settings_t settings = 0;

static void shell_stdout_callback(const char* output, size_t length, void* context) {
	(void)context;
	(void)length;
	(void)context;
	ansi_fprintf(stdout, ANSI_ITALIC ANSI_WHITE "%s", output);
}

static void shell_stderr_callback(const char* output, size_t length, void* context) {
	(void)context;
	(void)length;
	(void)context;
	ansi_fprintf(stderr, ANSI_ITALIC ANSI_RED "%s", output);
}

static void printBanner() {
	if( ! isatty(fileno(stdin)) ) return; // only print banner if stdin is a terminal
	ansi_fprintf(stdout, ANSI_BOLD "sercon - A Serial-Ports Console (v%s)\n", VERSION);
	ansi_fprintf(stdout, "(Type 'help' for a list of commands, and 'quit' to exit)\n");
}

/**
 * @brief Print the list of available serial ports.
 */
static void print_ports_list(void) {
	ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available serial ports:\n");
	iterator_t i = enumSerialPorts();
	_foreach(i, res) {
		const char* portName = (const char*)res.value;
		ansi_fprintf(stdout, ANSI_ITALIC "  %s\n", portName);
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
		FREE(port);
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
 * @brief Create a connection string from a port specification.
 * @param s The port specification in the format "PORT{:BAUD}".
 * @return char* The connection string in the format "PORT:BAUD", or NULL on failure.
 */
static char* makeConnectionString(const char *s) {
	char* out = NULL;
	char portName[256];
	int baudRate = 9600;
	if( ! s ) return NULL;
	int n = parseConnectionString(s, portName, sizeof(portName), &baudRate);
	if( n < 1 ) {
		a_error("Invalid port specification: %s\n", s);
		return NULL;
	}
	asprintf(&out, "%s:%d", portName, baudRate ? baudRate : 9600);
	return out;
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
	int n = parseConnectionString(connectionString, portName, sizeof(portName), &baudRate);
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
			char* connectionString = makeConnectionString(optarg);
			if( ! connectionString ) {
				a_error("Invalid port specification: %s\n", optarg);
				exit(1);
			}
			port = connectionString; // take ownership of the string
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
			char* connectionString = makeConnectionString(argv[1]);
			if( ! connectionString ) {
				a_error("Invalid connection string: %s\n", argv[1]);
				break;
			}
			fdPort = applyConnectionString(tc, argv[1]);
			if( fdPort > 0 ) {
				if( port ) free(port);
				port = connectionString; // take ownership of the string
				termctl_add_fd(tc, fdPort);
			} else {
				a_error("Failed to connect to %s\n", argv[1]);
				FREE(connectionString);
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
			run_script(tc, argv[1], false);
			break;
		}
		case 'B': { // shell
			if( argc < 2 ) {
				a_error("Usage: shell COMMAND\n");
				break;
			}
			ASSERT(argv[argc] == NULL); // ensure argv is null-terminated
			const char* shell = settings_get(settings, "shell");
			char* command = sc_shell_make_command(shell, argc - 1, argv + 1);
			if( ! command ) {
				a_error("Failed to construct shell command\n");
				break;
			}
			int err = sc_shell(command, NULL, shell_stdout_callback, shell_stderr_callback, tc);
			if( err ) {
				a_error("Shell command failed with exit code %d\n", err);
			}
			FREE(command);
			break;
		}
		case 'E': { // clear screen
			ansi_fprintf(stdout, ANSI_CLEAR_SCREEN ANSI_INIT_CURSOR_POS);
			printBanner();
			break;
		}
#ifdef _DEBUG_
		case 'A': { // show autocomplete vocabulary
			rlx_print_autocomplete_vocabulary(h);
			break;
		}
		case 'S': { // show settings
			settings_print(settings);
			break;
		}
#endif
	}
}

/**
 * @brief Prompt callback function for a termctl instance.
 * @param tc The termctl instance.
 * @param context User data pointer.
 * @return char* The prompt string.
 * @details termctl will free the returned string after use, so it should be dynamically allocated.
 */
static char* prompt_callback(termctl_t tc, void* context) {
	(void)tc;
	(void)context;
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
static void user_input_callback(termctl_t tc, const char* line, size_t length, void* context) {
	(void)tc;
	(void)context;
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
static void newline_callback(termctl_t tc, void* context) {
	(void)tc;
	(void)context;
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
static int reconnect_callback(termctl_t tc, int fd, void* context) {
	(void)context;
	(void)fd;
	ASSERT(tc);
	ASSERT(port);
	return fdPort = applyConnectionString(tc, port);
}

/**
 * @brief Autocomplete callback function that returns available port.
 * @param rlx The rlx instance.
 * @param text The current input text for which to provide autocomplete suggestions.
 * @param context User data pointer.
 */
static void autocomplete_ports_callback(rlx_t rlx, const char* text, void* context) {
	(void)context;
	(void)text;
	ASSERT(rlx);
	iterator_t i = enumSerialPorts();
	_foreach(i, res) {
		const char* portName = (const char*)res.value;
		rlx_add_autocomplete_vocabulary_entry(rlx, portName);
	}
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
		FREE(port);
		port = 0;
	}
	if( settings ) {
		settings_free(settings);
		settings = 0;
	}
}

static void abort_handler(int sig) {
	(void)sig;
	DEBUG_MSG("Aborting application due to signal %d", sig);
	on_exit_handler();
	exit(-1);
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

// registered commnands
static const rlx_registered_command_t commands[] = {
	{'i', "history", "Show command history", registered_commands_callback},
	{'c', "clear-history", "Clear history", registered_commands_callback},
	{'E', "clear-screen", "Clear the terminal screen", registered_commands_callback},
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
	RLX_REGISTERED_COMMANDS_END
};

int main(int argc, char* argv[]) {
	const char* appname = basename(argv[0]);

	settings = settings_init(appname);

	atexit(on_exit_handler);
	signal(SIGABRT, abort_handler);

	parse_cli_args(argc, argv);
	begin_ansi(false);
	apply_loaded_settings();

	printBanner();

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
	rlx_add_autocomplete_callback(termctl_get_rlx(termctl), autocomplete_ports_callback);

	rlx_register_commands(termctl_get_rlx(termctl), commands);

	// run rc script if it exists
	run_script(termctl, settings_get_rcfilename(settings), true);

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
