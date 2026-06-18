#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/param.h>
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
char* port = 0;
int baud = 9600;
bool printTimestamps = true;
bool bPrintPortsList = false;
int retrySeconds = 1;
int waitOnEOFSeconds = 1;
int maxHistoryEntries = 200;
bool bPersistentHistory = true;

// global state variables
char* appname = 0;
volatile int shouldAbort = 0;
volatile int pollTimeout = -1;
bool atFirstRetryCycle = true;
int fdPort = -1;
rlx_t rlx = 0;
char* prompt = 0;

static void print_port_callback(const char* portName, void* userData) {
	(void)userData;
	afprintf(stdout, ANSI_ITALIC "  %s\n", portName);
}

static void parse_option_callback(int pos, int opt, const char* optarg) {
	(void)pos; // unused for now, but could be useful for positional arguments in the future
	switch( opt ) {
		case 'l': {
			bPrintPortsList = true;
			break;
		}
		case 'p': {
			if( port ) free(port);
			port = strdup(optarg);
			break;
		}
		case 'b': {
			baud = MAX(atoi(optarg), 9600);
			break;
		}
		case 'T': {
			printTimestamps = false;
			break;
		}
		case 'v': {
			afprintf(stdout, "%s\n", VERSION);
			exit(0);
		}
		case 'r': {
			retrySeconds = MAX(atoi(optarg), 1);
			break;
		}
		case 'w': {
			waitOnEOFSeconds = MAX(atoi(optarg), 0);
			break;
		}
		case 'e': {
			maxHistoryEntries = MAX(atoi(optarg), 10);
			break;
		}
		case 'H': {
			bPersistentHistory = false;
			break;
		}
		case 0: {
			a_error("Unexpected positional argument: %s\n", optarg);
			exit(1);
		}
	} // end switch
}

static void on_sigint(int signum) {
	(void)signum;
	shouldAbort = 1;
}

static void on_exit_app(void) {
	if( rlx ) {
		// free RLX resources and commit history to disk if persistent history is enabled
		rlx_end(rlx);
		rlx = 0;
	}
	if( port ) {
		free(port);
		port = 0;
	}
	if( fdPort >= 0 ) {
		close(fdPort);
		fdPort = -1;
	}
	if( prompt ) {
		free(prompt);
		prompt = 0;
	}
}

// Parse command-line arguments
static void parse_args(int argc, char *argv[])
{
	static const char* description = "sercon - A simple serial console for Arduino development\n\n"
		"This tool allows you to connect to a serial port and interact with it in real-time.\n"
		"You can specify the port and baud rate, and it will display incoming data with optional\n"
		"timestamps. Use the --list option to see available serial ports on your system.";
	static const getopt_ex_option_t options[] = {
		{{"list", no_argument, 0, 'l'}, "List available serial ports", 0},
		{{"port", required_argument, 0, 'p'}, "Specify the serial port", "PORT"},
		{{"baud", required_argument, 0, 'b'}, "Specify the baud rate (default: 9600)", "BAUD-RATE"},
		{{"no-timestamps", no_argument, 0, 'T'}, "Disable timestamps", 0},
		{{"version", no_argument, 0, 'v'}, "Show version information", 0},
		{{"retry-delay", required_argument, 0, 'r'}, "Seconds to wait before retrying connection (default: 1)", "SECONDS"},
		{{"eof-wait", required_argument, 0, 'w'}, "Seconds to wait after EOF on stdin before exiting (default: 1)", "SECONDS"},
		{{"max-history", required_argument, 0, 'e'}, "Maximum number of history entries to keep (default: 200)", "NUMBER"},
		{{"non-persistent-history", no_argument, 0, 'H'}, "Disable persistent history (history will not be saved to a file)", 0},
		GETOPT_EX_OPTIONS_END
	};
	ASSERT(options[array_size(options)-1].opt.name == 0); // ensure the last option is the end marker
	getopt_ex(argc, argv, options, description, parse_option_callback);
}

static char* makePrompt() {
	if( ! isatty(fileno(stdin)) ) return 0; // no prompt if stdin is redirected!!
	char* p = 0;
	bool connected = fdPort >= 0;
	const char* portNoPath = strrchr(port, '/') + 1;
	// set the prompt to the app name if not connected to a serial port yet,
	// otherwise use the port name (without any leading path) as the prompt
	asprintf(&p, ANSI_BLUE ANSI_ITALIC "%s> " ANSI_RESET, connected ? portNoPath : appname);
	return p;
}

static int printTimestamp(FILE* stream) {
	cal_time_t t;
	now(&t);
	return afprintf(stream, ANSI_CYAN"[%02d:%02d:%02d.%03d] ",
		t.hours, t.minutes, t.seconds, t.milliseconds);
}

static void termCmdCallback(
		rlx_t h, const rlx_registered_command_t* cmd, int argc, const char* argv[], void* userData) {
	(void)userData;
	switch( cmd->id ) {
		case 'h': {
			if( argc > 1 ) {
				// get help for specific commands
				for(int i=1; i<argc; i++) {
					const rlx_registered_command_t* cmd = rlx_get_command(h, argv[i]);
					if( cmd ) {
						afprintf(stdout, "%s - %s\n", cmd->command, cmd->description);
					} else {
						a_error("No such command: %s\n", argv[i]);
					}
				}
			} else {
				// overall help message
				afprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available commands:\n");
				rlx_print_registered_commands(h);
			}
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
		case 'i': {
			rlx_print_history(h);
			break;
		}
	}
}
static void setupTerminalCommands() {
	static const rlx_registered_command_t commands[] = {
		{'i', "history", "Show command history", termCmdCallback},
		{'c', "clear", "Clear history", termCmdCallback},
		{'h', "help", "Show this help message", termCmdCallback},
		{'q', "quit", "Exit the program", termCmdCallback},
		{0, 0, 0, 0} // end marker
	};
	ASSERT(rlx);
	ASSERT(commands[array_size(commands)-1].command == 0); // ensure the last command is the end marker
	rlx_register_commands(rlx, commands);
}

void rlx_callback(rlx_t h, char* line, size_t length) {
	(void)h;
	if( ! line ) {
		if( isatty(fileno(stdin)) ) {
			// interactive mode - exit immediately
			raise(SIGINT);
		} else {
			// allow some time for a response from the device before exiting
			pollTimeout = waitOnEOFSeconds * 1000; // set a timeout to allow any pending serial output to be printed before we exit
		}
		return;
	}
	if( ! rlx_process_command(rlx, line, 0) ) {
		write(fdPort, line, length);
		write(fdPort, "\n", 1);
	}
	// (RLX will free the line buffer for us)
}

// Main console loop to concurrently handle input from both the serial port and user input
static void console() {
	char buffer[128];
	int fdStdin=fileno(stdin);
	bool atNewLine = true; // used to track if the next character is at the start of a new line (for timestamping)
	bool interactive = isatty(fdStdin) && isatty(fileno(stdout));

	// Open the serial port
	fdPort = open(port, O_RDWR | O_NOCTTY);
	if( fdPort < 0 ) {
		if( atFirstRetryCycle ) {
			a_error("Error opening serial port " ANSI_BOLD "%s" ANSI_POP ". Retrying...\n", port);
			atFirstRetryCycle = false;
		}
		return;
	}
	// connection successful, reset retry flag so that we will print an error message
	// if the connection is lost later
	atFirstRetryCycle = true;

	if( interactive ) {
		a_info("Connected to " ANSI_BOLD "%s" ANSI_POP " at %d baud\n", port, baud);
		a_info("(Press Ctrl+C to exit)\n");
		prompt = makePrompt();
	}

	// set baud rate for the serial port
	struct termios t;
	tcgetattr(fdPort, &t);
	cfsetspeed(&t, baud);
	tcsetattr(fdPort, TCSANOW, &t);

	struct pollfd fds[] = {
		{ .fd = fdPort, .events = POLLIN },
		{ .fd = fdStdin, .events = POLLIN },
	};

	// initialize readline for handling user input and command history
	rlx = rlx_begin(appname, prompt, rlx_callback, maxHistoryEntries, 0,
		(bPersistentHistory ? RLX_OPT_PERSIST_HISTORY : 0) | RLX_OPT_AUTOCOMPLETE_COMMANDS | RLX_OPT_AUTOCOMPLETE_HISTORY);
	if( ! rlx ) {
		a_error("Error initializing RLX session\n");
		exit(1);
	}

	setupTerminalCommands();

	while( ! shouldAbort ) {
		// wait for input from either stdin (fds[1]) or the serial port (fds[0]).
		// if stdin is at EOF while in non-interactive mode, we will ignore stdin
		// and set a final timeout to allow any pending serial output to be printed before we exit.
		int ret = poll(fds, array_size(fds) - (pollTimeout >= 0 ? 1 : 0), pollTimeout);

		// Check for poll errors
		if( ret <= 0 ) {
			break;
		}

		// Check if user input is available
		if( fds[1].revents & POLLIN ) {
			rlx_process_input(rlx); // this will trigger readline to read the input and call our RLX callback function
		} // end stdin handling

		// Check if data is available from the serial port
		if( fds[0].revents & POLLIN ) {
			ssize_t bytesRead = read(fdPort, buffer, sizeof(buffer) - 1);
			if( bytesRead < 0 ) {
				break;
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

	rlx_end(rlx);
	rlx = 0;

	close(fdPort);
	fdPort = -1;

	free(prompt);
	prompt = 0;
}

int main(int argc, char *argv[])
{
	appname = basename(argv[0]);

	// process command-line arguments
	parse_args(argc, argv);
	if( bPrintPortsList ) {
		afprintf(stdout, ANSI_UNDERLINE ANSI_BOLD "Available serial ports:\n");
		int n = enumSerialPorts(print_port_callback, 0);
		if( ! n ) {
			afprintf(stdout, ANSI_ITALIC "  No serial ports found\n");
		}
		exit(0);
	}
	if( ! port ) {
		a_error("Error: Serial port not specified\n");
		exit(1);
	}

	begin_ansi(false);
	atexit(on_exit_app);
	signal(SIGINT, on_sigint);

	// Main console loop
	for(;;) {
		console();
		if( shouldAbort || ! isatty(fileno(stdin)) ) {
			break;
		}
		sleep(retrySeconds);
	}

	return 0;
}
