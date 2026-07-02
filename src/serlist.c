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
 * @param callback The callback function to be called for each available serial port.
 * @param context User-defined data to be passed to the callback function.
 * @return r_array_t An array of available serial ports.
 */
r_array_t enumSerialPorts() {
	r_array_t ports = r_array_create(0, free);
	char* paths = 0;
	const char* envPats = getenv(ENV_NAME);
	asprintf(&paths, "%s:%s", def_paths, envPats ? envPats : "");
	r_array_t path_array = parse_path_list(paths);
	for(size_t i=0; i < r_array_size(path_array); ++i) {
		cglob_iterator_t g = globIterator(r_array_get(path_array, i), CGLOB_FILE_CHAR_DEVICE);
		for(const char* path = nextGlob(g); path; path = nextGlob(g)) {
			r_array_add(ports, strdup(path));
		}
		freeGlobIterator(g);
	}
	r_array_destroy(path_array);
	free(paths);
	return ports;
}
