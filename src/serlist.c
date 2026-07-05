#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"
#include "serlist.h"
#include "command.h"
#include "cglob.h"

#define ENV_NAME "PORTS_SEARCH_PATH"

#ifdef __APPLE__
static const char* def_paths = "/dev/cu.*";
#elif __linux__
static const char* def_paths = "/dev/ttyUSB*:/dev/ttyACM*:/dev/ttyS*:/dev/bus/usb/*";
#else
#	warning "Unsupported platform for serial port enumeration. No knowledge of default search paths."
static const char* def_paths = "";
#endif


/**
 * @brief Warns if the PORTS_SEARCH_PATH environment variable is set, overriding default patterns.
 */
INITIALIZER(static void warn_about_overriding_env_patterns()) {
	const char* env_patterns = getenv(ENV_NAME);
	if( env_patterns ) {
		DEBUG_MSG("${%s} is set and will be added to the default patterns: %s", ENV_NAME, env_patterns);
	}
}

/**
 * @brief Enumerates the available serial ports on the system.
 * @return iterator_t An iterator over the available serial ports.
 */
iterator_t enumSerialPorts() {
	char* paths = 0;

	// build paths from default and environment variable
	const char* envPats = getenv(ENV_NAME);
	asprintf(&paths, "%s:%s", def_paths, envPats ? envPats : "");

	// convert to a dynamic array of strings
	r_array_t path_array = parse_path_list(paths);
	if( ! path_array ) {
		free(paths);
		return NULL;
	}

	iterator_t g = cglob_iterator((const char**)r_array_elements(path_array), CGLOB_FILE_CHAR_DEVICE);

	// cleanup
	r_array_destroy(path_array);
	free(paths);

	return g;
}
