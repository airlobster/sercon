#include <stddef.h>
#include <string.h>
#include "utils.h"
#include "serlist.h"

/**
 * @brief Enumerates the available serial ports on the system.
 * @param callback The callback function to be called for each available serial port.
 * @param userData User-defined data to be passed to the callback function.
 * @return int The number of available serial ports.
 */
int enumSerialPorts(void(*callback)(const char* port, void* userData), void* userData) {
	ASSERT(callback);
#ifdef __APPLE__
	static const char* patterns[] = { "/dev/cu.*" };
#elif __linux__
	static const char* patterns[] = { "/dev/ttyUSB*", "/dev/ttyACM*", "/dev/bus/usb/*" };
#else
	#error "Unsupported platform for serial port enumeration"
#endif
	int n = 0;
	for(size_t i=0; i < array_size(patterns); ++i) {
		n += cglob(patterns[i], callback, userData);
	}
	return n;
}
