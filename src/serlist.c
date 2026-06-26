#include <stddef.h>
#include <string.h>
#include "utils.h"
#include "serlist.h"
#include "command.h"

static void warn_about_overriding_env_patterns() __attribute__((constructor));
static void warn_about_overriding_env_patterns() {
	const char* env_patterns = getenv("SERIAL_PORT_PATTERNS");
	if( env_patterns ) {
		DEBUG_MSG("SERIAL_PORT_PATTERNS environment variable is set, overriding default serial port patterns: %s", env_patterns);
	}
}

/**
 * @brief Enumerates the available serial ports on the system.
 * @param callback The callback function to be called for each available serial port.
 * @param userData User-defined data to be passed to the callback function.
 * @return int The number of available serial ports.
 */
int enumSerialPorts(void(*callback)(const char* port, void* userData), void* userData) {
#ifdef __APPLE__
	static const char* patterns[] = { "/dev/cu.*" };
#elif __linux__
	static const char* patterns[] = { "/dev/ttyUSB*", "/dev/ttyACM*", "/dev/ttyS*", "/dev/bus/usb/*" };
#else
#	error "Unsupported platform for serial port enumeration"
#endif
	int n = 0;
	ASSERT(callback);
	const char* env_patterns = getenv("SERIAL_PORT_PATTERNS");
	if( env_patterns ) {
		// preset patterns are overridden by the environment variable,
		// which is a space-separated list of patterns
		int argc = 0;
		char** argv = 0;
		parse_command_line(env_patterns, &argc, &argv);
		for(int i=0; i < argc; ++i) {
			n += cglob(argv[i], callback, userData);
		}
		free_command_args(argc, argv);
	} else {
		for(size_t i=0; i < array_size(patterns); ++i) {
			n += cglob(patterns[i], callback, userData);
		}
	}
	return n;
}
