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

    ./sercon --port cu.usbmodem1101

Connect with explicit baud:

    ./sercon --port cu.usbmodem1101 --baud 115200

Baud values below 9600 are clamped to 9600.

Disable timestamp prefix on incoming lines:

    ./sercon --port cu.usbmodem1101 --no-timestamps

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

## Behavior notes

- Terminal-like behavior when STDIN is not redirected. That also includes port-specific
history management.
- Ctrl+C exits cleanly.
- The app uses polling to handle both terminal input and serial input.
- Prompt text defaults to the selected port name (or the executable name if no port basename is available).
- Prompt and cursor UI are shown only in interactive mode.
- Carriage returns (\r) are ignored, linefeeds drive line/timestamp boundaries.
