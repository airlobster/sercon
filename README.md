# sercon

sercon is a small serial terminal for Arduino and other USB/UART devices.

It supports:
- Interactive stdin <-> serial forwarding
- Optional timestamp prefix on incoming lines
- ANSI-styled output with automatic TTY detection
- Serial port discovery through libserialport
- Auto-complete
- History

## Requirements

- A C toolchain (cc)
- make
- libserialport
- libreadline
- pkg-config (recommended)

On macOS with Homebrew:

    brew install libserialport pkg-config readline

## Usage

List available serial ports:

    ./sercon --list

Connect to a port at default baud (9600):

    ./sercon --port /dev/cu.usbmodem1101

Connect with explicit baud rate:

    ./sercon --port cu.usbmodem1101 --baud 115200

Baud values below 9600 are clamped to 9600.

Disable timestamp prefix on incoming lines:

    ./sercon --port /dev/cu.usbmodem1101 --no-timestamps

Print version and exit:

    ./sercon --version

Show help and exit:

    ./sercon --help

Short flags are also available:
- -l for --list
- -h for --help
- -p for --port
- -b for --baud
- -T for --no-timestamps
- -v for --version
- -r for --retry-delay
- -w for --eof-wait
- -H for --non-persistent-history
- -e for --max-history

## Installation

After cloning this repository, just enter the repository's root directory and run

	./INSTALL

This will build the project and install it. To test for installation correctness, run

	make test


## Behavior notes

- Terminal-like behavior when STDIN is not redirected. That also includes port-specific
history management.
- Ctrl+C exits cleanly.
- The app uses polling to handle both terminal input and serial input.
- Prompt text defaults to the selected port name (or the executable name if no port basename is available).
- Prompt and cursor UI are shown only in interactive mode.
- Carriage returns (\r) are ignored, linefeeds drive line/timestamp boundaries.

## Appendix

### Reussable modules

#### readline_ex.h, readline_ex.c

readline_ex is a wrapper around the GNU readline library that provides some additional features, such as:
+ Simpler API
+ Support for registering commands with associated handlers and descriptions,
	and processing user input to execute those commands.
+ Support for auto-completion.
+ Default auto-completion vocabulary that can include both registered commands
	and command history entries, with options to configure this to your needs.
+	Ability to set a custom auto-completion vocabulary.
+ Support for pausing and resuming readline to allow printing output from other
	file-descriptors without interfering with the user's current input.
+ Support for persisting command history to a file and loading it on startup,
	with optional context-based history files.
*	Full shell-like history management support.

#### getopt_ex.h, getopt_ex.c

Allows declaring command-line arguments with automatic help information generation.

#### command.h, command.c

Shell-like command-line parser - translate a raw string into argc/argv duo.
