#if !defined(__APPLE__) && !defined(__linux__)
#error "Unsupported platform"
#endif

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/poll.h>
#include <string.h>
#include <termios.h>
#include "ansi.h"
#include "serlist.h"
#include "getopt_ex.h"
#include "utils.h"
#include "readline_ex.h"

#ifndef VERSION
#define VERSION "0.0.1"
#endif

// global configuration variables
char *port = 0;
int baud = 9600;
bool printTimestamps = true;
int pollTimeoutMs = 500;
int maxHistoryEntries = 200;
bool bPersistentHistory = true;

// global state variables
char *appname = 0;
int fdPort = -1;
rlx_t rlx = 0;
char *prompt = 0;
volatile int shouldAbort = 0;
int firstConnectionError = true;

/**
 * @brief Context structure for the terminal.
 * @details This structure holds the file descriptors for polling.
 */
typedef struct {
	/** The file descriptors for polling. */
	struct pollfd fds[2];
} terminal_context_t;

static void add_ports_to_vocabulary_callback(const char* portName, void* userData) {
	rlx_t h = (rlx_t)userData;
	rlx_add_autocomplete_vocabulary_entry(h, portName);
}

static void print_port_callback(const char* portName, void* userData) {
	(void)userData;
	ansi_fprintf(stdout, ANSI_ITALIC "  %s\n", portName);
}

static void print_ports_list() {
	ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available serial ports:\n");
	int n = enumSerialPorts(print_port_callback, 0);
	if( ! n ) {
		ansi_fprintf(stdout, ANSI_ITALIC "  No serial ports found\n");
	}
}

static char* makePrompt(char** outPrompt) {
	ASSERT(outPrompt);
	if( ! isatty(fileno(stdin)) ) return *outPrompt; // no prompt if stdin is redirected!!
	char *p = 0, *pSafe = 0;
	if( port ) {
		bool connected = fdPort >= 0;
		const char* portNoPath = strrchr(port, '/') + 1;
		if( connected ) {
			// connected
			ansi_asprintf(&p, ANSI_BLUE ANSI_ITALIC "%s:%d> ", portNoPath, baud);
		} else {
			// connection lost
			ansi_asprintf(&p, ANSI_WARNING ANSI_ITALIC "%s...> ", portNoPath);
		}
	} else {
		// disconnected
		ansi_asprintf(&p, ANSI_BLUE ANSI_DIM ANSI_ITALIC "%s> ", "not-connected");
	}
	rlx_make_safe_prompt(p, &pSafe);
	free(p);
	// replace the old prompt with the new one, freeing the old prompt if necessary
	if( *outPrompt ) free(*outPrompt);
	*outPrompt = pSafe;
	return pSafe;
}

static int printTimestamp(FILE* stream) {
	cal_time_t t;
	now(&t);
	return ansi_fprintf(stream, ANSI_CYAN "[%02d:%02d:%02d.%03d] ",
		t.hours, t.minutes, t.seconds, t.milliseconds);
}

static void disconnect(terminal_context_t* termContext) {
	if( fdPort >= 0 ) {
		close(fdPort);
		fdPort = -1;
		if( termContext )	termContext->fds[0].fd = -1;
	}
	if( port ) {
		free(port);
		port = 0;
	}
}

static bool connect(terminal_context_t* termContext, const char* portName, int baudRate) {
	// disconnect(termContext);
	ASSERT(fdPort < 0); // should only call connect() when not currently connected
	if( fdPort > 0 ) {
		a_error("Already connected to a port. Please disconnect first.\n");
		return false;
	}
	fdPort = open(portName, O_RDWR | O_NOCTTY);
	if( fdPort < 0 ) {
		if( firstConnectionError ) {
			a_error("Error opening serial port '%s': %s\n", portName, strerror(errno));
			firstConnectionError = false;
		}
		return false;
	}
	firstConnectionError = true;
	if( termContext ) termContext->fds[0].fd = fdPort;
	port = strdup(portName);
	baud = baudRate;
	// set baud rate
	struct termios t;
	tcgetattr(fdPort, &t);
	cfsetspeed(&t, baudRate);
	tcsetattr(fdPort, TCSANOW, &t);
	return true;
}

static bool applyConnectionString(terminal_context_t* termContext, const char *connectionString) {
	char portName[256];
	int baudRate = 9600;
	int n = sscanf(connectionString, "%[^:]:%d", portName, &baudRate);
	if( n < 1 ) {
		a_error("Invalid port specification: %s\n", connectionString);
		return false;
	}
	return connect(termContext, portName, MAX(baudRate, 9600));
}

static void registered_commands_callback(
		rlx_t h, const rlx_registered_command_t* cmd, int argc, const char* argv[], void* userData) {
	terminal_context_t* termContext = (terminal_context_t*)userData;
	(void)termContext; // currently unused, but could be useful for future enhancements
	switch( cmd->id ) {
		case 'h': {
			if( argc > 1 ) {
				// get help for specific commands
				for(int i=1; i<argc; i++) {
					const rlx_registered_command_t* cmd = rlx_get_command(h, argv[i]);
					if( cmd ) {
						ansi_fprintf(stdout, "%s - %s\n", cmd->command, cmd->description);
					} else {
						a_error("No such command: %s\n", argv[i]);
					}
				}
			} else {
				// overall help message
				ansi_fprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available commands:\n");
				rlx_print_registered_commands(h);
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
			ansi_fprintf(stdout, "%s\n", VERSION);
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
			applyConnectionString(termContext, argv[1]);
			break;
		}
		case 'D': {
			if( fdPort < 0 ) {
				a_error("Not currently connected to any port\n");
				break;
			}
			disconnect(termContext);
			break;
		}
		case 't': {
			if( argc > 1 ) {
				if( strcasecmp(argv[1], "on") == 0 ) {
					printTimestamps = true;
				} else if( strcasecmp(argv[1], "off") == 0 ) {
					printTimestamps = false;
				} else {
					a_error("Usage: timestamp [on|off]\n");
					break;
				}
			}
			a_success("Timestamps %s\n", printTimestamps ? "on" : "off");
			break;
		}
#ifdef _DEBUG_
		case 'A': {
			rlx_print_autocomplete_vocabulary(rlx);
			break;
		}
#endif
	}
}
static void setupTerminalRegisteredCommands() {
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
#ifdef _DEBUG_
		{'A', "vocabulary", "Show auto-complete vocabulary (debugging only)", registered_commands_callback},
#endif
		{0, 0, 0, 0} // end marker
	};
	ASSERT(rlx);
	ASSERT(commands[array_size(commands)-1].command == 0); // ensure the last command is the end marker
	rlx_register_commands(rlx, commands);
}

// RLX callback function to handle user input from the console
void rlx_callback(rlx_t h, const char* line, size_t length, void* userData) {
	(void)h;
	terminal_context_t* termContext = (terminal_context_t*)userData;
	ASSERT(termContext);
	if( ! line ) {
		termContext->fds[1].fd = -1; // tell poll we're done reading from stdin (EOF)
		return;
	}
	// if the line is not a recognized command, forward it to the serial port
	if( ! rlx_process_command(rlx, line) && length > 0 ) {
		if( fdPort > 0 ) {
			write(fdPort, line, length);
			write(fdPort, "\n", 1);
		}	else {
			a_error("Not connected to any serial port. Use the 'connect' command to connect.\n");
		}
	}
}

// Main console loop to concurrently handle input from both the serial port and user input
static void console() {
	char buffer[128];
	int fdStdin=fileno(stdin);
	bool atNewLine = true; // used to track if the next character is at the start of a new line (for timestamping)

	terminal_context_t termContext = {
		.fds = {
			{ .fd = fdPort, .events = POLLIN },
			{ .fd = fdStdin, .events = POLLIN },
		},
	};

	// create a readline_ex session for handling user input, command history and auto-completion
	const unsigned long opt =
		(bPersistentHistory ? RLX_OPT_PERSIST_HISTORY : 0)
		| (RLX_OPT_AUTOCOMPLETE_COMMANDS /*| RLX_OPT_AUTOCOMPLETE_HISTORY*/)
		;
	rlx = rlx_begin(appname, 0, rlx_callback, maxHistoryEntries, 0, opt, &termContext);
	if( ! rlx ) {
		a_error("Error initiating RLX session\n");
		exit(1);
	}

	setupTerminalRegisteredCommands();

	// add available ports to the autocomplete vocabulary for convenience
	// (we do this after initializing RLX so that the readline state is properly set up for handling dynamic vocabulary updates)
	enumSerialPorts(add_ports_to_vocabulary_callback, rlx);

	while( ! shouldAbort ) {
		int ret = poll(termContext.fds, array_size(termContext.fds), pollTimeoutMs);

		// Check for poll errors
		if( ret < 0 ) {
			if( ! shouldAbort ) {
				a_error("Error during poll: %s\n", strerror(errno));
			}
			break;
		}

		// check for timeout
		if( ret == 0 ) {
			if( termContext.fds[1].fd < 0 ) {
				// STDIN was closed (EOF)
				break;
			}
			// if we have a port specified but are not currently connected,
			// it means we have lost the connection unexpectedly. retry to connect.
			if( port && fdPort < 0 ) {
				connect(&termContext, port, baud);
			}
			// update the prompt periodically
			// to reflect the current connection status (connected/disconnected)
			makePrompt(&prompt);
			rlx_change_prompt(rlx, prompt);
			continue;
		}

		// Check if user input is available
		if( termContext.fds[1].revents & POLLIN ) {
			// trigger readline_ex to read the input and call our RLX callback function
			rlx_process_input(rlx);
		} // end stdin handling

		// Check if data is available from the serial port
		if( termContext.fds[0].revents & POLLIN ) {
			ssize_t bytesRead = read(termContext.fds[0].fd, buffer, sizeof(buffer) - 1);
			if( bytesRead < 0 ) {
				// a_error("Error reading from serial port: %s\n", strerror(errno));
				termContext.fds[0].fd = fdPort = -1; // mark the serial port as disconnected
				continue;
			}

			rlx_pause(rlx);

			buffer[bytesRead] = '\0';
			for(const char* p = buffer; *p; p++) {
				if( *p == '\n' ) {
					atNewLine = true; // next character will be preceeded by a timestamp
					fputc('\n', stdout);
				} else if( *p == '\r' ) {
					// ignore
				} else {
					if( atNewLine ) {
						// print timestamp at the start of each new line (if so configured)
						if( printTimestamps ) {
							printTimestamp(stdout);
						}
						atNewLine = false;
					}
					fputc(*p, stdout);
				}
			} // end for
			fflush(stdout);

			rlx_resume(rlx, atNewLine);
		} // end serial port handling
	} // end while
}

static void cli_args_callback(int pos, int opt, const char* optarg) {
	(void)pos; // unused for now, but could be useful for positional arguments in the future
	switch( opt ) {
		case 'l': {
			print_ports_list();
			exit(0);
		}
		case 'p': {
			if( ! applyConnectionString(0, optarg) ) {
				exit(1);
			}
			break;
		}
		case 'T': {
			printTimestamps = false;
			break;
		}
		case 'v': {
			ansi_fprintf(stdout, "%s\n", VERSION);
			exit(0);
		}
		case 'e': {
			maxHistoryEntries = MAX(atoi(optarg), 10);
			break;
		}
		case 'H': {
			bPersistentHistory = false;
			break;
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
		{{"max-history", required_argument, 0, 'e'}, "Maximum number of history entries to keep (default: 200)", "NUMBER"},
		{{"non-persistent-history", no_argument, 0, 'H'}, "Disable persistent history (history will not be saved to a file)", 0},
		{{"color", required_argument, 0, 'c'}, "Set ANSI color mode (auto|always|never)", "MODE"},
		GETOPT_EX_OPTIONS_END
	};
	ASSERT(options[array_size(options)-1].opt.name == 0); // ensure the last option is the end marker
	getopt_ex(argc, argv, options, description, cli_args_callback);
}

static void on_signal(int signum) {
	(void)signum;
	shouldAbort = 1;
	fputc('\n', stdout);
}

static void on_exit_app(void) {
	DEBUG_MSG("Shutting down application");
	if( rlx ) {
		rlx_end(rlx);
		rlx = 0;
	}
	disconnect(0);
	if( prompt ) {
		free(prompt);
		prompt = 0;
	}
}

int main(int argc, char *argv[])
{
	appname = basename(argv[0]);

	// process command-line arguments
	parse_cli_args(argc, argv);

	begin_ansi(false);
	atexit(on_exit_app);
	signal(SIGTERM, on_signal);
	signal(SIGSEGV, on_signal);
	signal(SIGINT, on_signal);

	console();

	return 0;
}
