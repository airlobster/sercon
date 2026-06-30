#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "utils.h"
#include "serlist.h"
#include "command.h"

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
 * @param userData User-defined data to be passed to the callback function.
 * @return int The number of available serial ports.
 */
int enumSerialPorts(void(*callback)(const char* port, void* userData), void* userData) {
	char* paths = 0;
	char** path_list = 0;
	int nPaths = 0, n = 0;
	ASSERT(callback);
	const char* envPats = getenv(ENV_NAME);
	asprintf(&paths, "%s:%s", def_paths, envPats ? envPats : "");
	parse_path_list(paths, &nPaths, &path_list);
	for(int i=0; i < nPaths; ++i) {
		n += cglob(path_list[i], CGLOB_FILE_CHAR_DEVICE, callback, userData);
		free(path_list[i]);
	}
	free(path_list);
	free(paths);
	return n;
}
