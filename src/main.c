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

#ifndef VERSION
#define VERSION "0.0.0.0"
#endif

int baud = 9600;
bool printTimestamps = true;
char* port = 0;
int fdPort = -1;
bool firstConnectionError = true;
termctl_t termctl = 0;

/**
 * @brief Update the prompt for a termctl instance.
 *
 * @param tc The internal termctl instance.
 */
static void add_ports_to_vocabulary_callback(const char* portName, void* userData) {
	termctl_t tc = (termctl_t)userData;
	ASSERT(tc);
	rlx_add_autocomplete_vocabulary_entry(termctl_get_rlx(tc), portName);
}

/**
 * @brief Print a serial port name.
 *
 * @param portName The name of the serial port.
 * @param userData User data pointer.
 */
static void print_port_callback(const char* portName, void* userData) {
	(void)userData;
	ansi_fprintf(stdout, ANSI_ITALIC "  %s\n", portName);
}

/**
 * @brief Print the list of available serial ports.
 */
static void print_ports_list() {
	ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available serial ports:\n");
	int n = enumSerialPorts(print_port_callback, 0);
	if( ! n ) {
		ansi_fprintf(stdout, ANSI_ITALIC "  No serial ports found\n");
	}
}

/**
 * @brief Connect to a serial port.
 *
 * @param tc The termctl instance.
 * @param portName The name of the serial port.
 * @param baudRate The baud rate for the connection.
 * @return int The file descriptor of the opened port, or -1 on failure.
 */
static int connect(termctl_t tc, const char* portName, int baudRate) {
	// disconnect(tc);
	ASSERT(portName);
	ASSERT(baudRate > 0);
	int fd = open(portName, O_RDWR | O_NOCTTY);
	if( fd < 0 ) {
		if( firstConnectionError ) {
			a_error("Error opening serial port '%s': %s\n", portName, strerror(errno));
			firstConnectionError = false;
		}
		return -1;
	}
	firstConnectionError = true;
	if( tc ) {
		termctl_add_fd(tc, fd);
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
 *
 * @param tc The termctl instance.
 */
static void disconnect(termctl_t tc) {
	ASSERT(tc);
	if( fdPort > 0 ) {
		close(fdPort);
		termctl_remove_fd(tc, fdPort);
		fdPort = -1;
	}
	if( port ) {
		free(port);
		port = 0;
	}
}

/**
 * @brief Apply a connection string to connect to a serial port.
 *
 * @param tc The termctl instance.
 * @param connectionString The connection string in the format "PORT{:BAUD}".
 * @return int The file descriptor of the opened port, or -1 on failure.
 */
static int applyConnectionString(termctl_t tc, const char *connectionString) {
	ASSERT(tc);
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
 *
 * @param tc The internal termctl instance.
 */
static void cli_args_callback(int pos, int opt, const char* optarg) {
	(void)pos;
	switch( opt ) {
		case 'l': {
			print_ports_list();
			exit(0);
		}
		case 'p': {
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
 *
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
 *
 * @param h The rlx handle.
 * @param cmd The registered command.
 * @param argc The argument count.
 * @param argv The argument vector.
 * @param userData User data pointer.
 */
static void registered_commands_callback(
		rlx_t h, const rlx_registered_command_t* cmd, int argc, const char* argv[], void* userData) {
	termctl_t tc = (termctl_t)userData;
	ASSERT(h);
	switch( cmd->id ) {
		case 'h': {
			ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available commands:\n");
			rlx_print_registered_commands(h);
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
		case 'C': {
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
			}
			break;
		}
		case 'D': {
			if( fdPort < 0 ) {
				a_error("Not currently connected to any port\n");
				break;
			}
			disconnect(tc);
			break;
		}
		case 't': {
			if( argc > 1 ) {
				printTimestamps = strcasecmp(argv[1], "on") == 0;
			}
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
			a_success("ANSI color mode: %s\n", get_ansi_mode());
			break;
		}
#ifdef _DEBUG_
		case 'A': {
			rlx_print_autocomplete_vocabulary(h);
			break;
		}
#endif
	}
}

/**
 * @brief Setup the registered commands for the terminal.
 *
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
#ifdef _DEBUG_
		{'A', "vocabulary", "Show auto-complete vocabulary (debugging only)", registered_commands_callback},
#endif
		{0, 0, 0, 0} // end marker
	};
	ASSERT(termctl);
	ASSERT(commands[array_size(commands)-1].command == 0); // ensure the last command is the end marker
	rlx_register_commands(termctl_get_rlx(termctl), commands);
}

/**
 * @brief Prompt callback function for a termctl instance.
 *
 * @param tc The termctl instance.
 * @param userData User data pointer.
 * @return char* The prompt string.
 */
static char* prompt_callback(termctl_t tc, void* userData) {
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
 *
 * @param tc The termctl instance.
 * @param line The input line.
 * @param length The length of the input line.
 * @param userData User data pointer.
 */
void user_input_callback(termctl_t tc, const char* line, size_t length, void* userData) {
	if( fdPort > 0 ) {
		write(fdPort, line, length);
		write(fdPort, "\n", 1);
	} else {
		a_error("Not connected to any serial port. Use 'connect <PORT>' to connect.\n");
	}
}

/**
 * @brief Newline callback function for a termctl instance.
 *
 * @param tc The termctl instance.
 * @param userData User data pointer.
 */
void newline_callback(termctl_t tc, void* userData) {
	if( printTimestamps ) {
		cal_time_t t;
		now(&t);
		ansi_fprintf(stdout, ANSI_INFO "[%02d:%02d:%02d.%03d] ",
			t.hours, t.minutes, t.seconds, t.milliseconds);
	}
}

int reconnect_callback(termctl_t tc, void* userData) {
	(void)userData;
	ASSERT(tc);
	ASSERT(port);
	return applyConnectionString(tc, port);
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
}

int main(int argc, char* argv[]) {
	const char* appname = basename(argv[0]);

	atexit(on_exit_handler);

	parse_cli_args(argc, argv);
	begin_ansi(false);

	// print banner (only if stdin is a terminal)
	if( isatty(fileno(stdin)) ) {
		ansi_fprintf(stdout, ANSI_BOLD "%s - A Serial-Ports Console (v%s)\n", appname, VERSION);
	}

	termctl = termctl_create(appname, 0);
	if( ! termctl ) {
		a_error("Failed to create termctl instance.\n");
		return 1;
	}

	// set callbacks for termctl
	termctl_set_prompt_callback(termctl, prompt_callback);
	termctl_set_user_input_callback(termctl, user_input_callback);
	termctl_set_newline_callback(termctl, newline_callback);
	termctl_set_reconnect_callback(termctl, reconnect_callback);
	setupTerminalRegisteredCommands(termctl);

	enumSerialPorts(add_ports_to_vocabulary_callback, termctl);

	// if port was specified in the command-line, attempt to connect to it
	// before entering the event loop
	if( port ) {
		ASSERT(fdPort < 0); // should only call connect() when not currently connected
		fdPort = applyConnectionString(termctl, port);
		if( fdPort <= 0 ) {
			a_error("Failed to connect to %s\n", port);
		}
	}

	return termctl_event_loop(termctl);
}
