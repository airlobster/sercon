# sercon

sercon is a small serial terminal for for communicating with serial ports.

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
- libreadline

On macOS with Homebrew:

    brew install readline

## Usage

List available serial ports:

    ./sercon --list

Connect to a port at default baud (9600):

    ./sercon --port /dev/cu.usbmodem1101

Connect with explicit baud rate:

    ./sercon --port cu.usbmodem1101:115200

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
- -T for --no-timestamps
- -v for --version
- -H for --non-persistent-history
- -e for --max-history

## Installation

After cloning this repository, just enter the repository's root directory and run

	make install


## Behavior notes

- Terminal-like behavior when STDIN is not redirected.
- From within the app, type 'help' to see the various commands supported.
- Ctrl+C exits cleanly.

## Serial-ports listing

Listing available serial-ports occures either when running sercon with the -l|--list option,
or with the 'ports' command from sercon's shell.
Serial-ports patterns being used:
- **MacOS**:
	- ```/dev/cu.*```
- **Linux**:
	- ```/dev/ttyUSB*```
	- ```/dev/ttyACM*```
	- ```/dev/ttyS*```
	- ```/dev/bus/usb/*```

Additionally, the pattern-sets above can be extended by using the ```PORTS_SEARCH_PATH``` environment variable, which is formatted like a ```PATH``` string, using a colon (:) as a delimiter.


## Appendix

### Reussable modules

#### iterator.h, iterator.c

An API for creating iterator objects.

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

### shell.h, shell.c

Execute shell commands while interceptiny standard devices.

### cglob.h, cglob.c

Multi-pattern, iterator-based globbing.

### d_XXXX.h, d_XXXX.c

Data structures utilized by sercon.
