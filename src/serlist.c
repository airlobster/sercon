#include <string.h>
#include "utils.h"
#include "serlist.h"

// https://github.com/sigrokproject/libserialport/releases/download/libserialport-0.1.2/libserialport-0.1.2.tar.gz
#include <libserialport.h>

/**
 * @brief Enumerates the available serial ports on the system.
 * @param callback The callback function to be called for each available serial port.
 * @param userData User-defined data to be passed to the callback function.
 * @return int The number of available serial ports.
 */
int enumSerialPorts(void(*callback)(const char* port, void* userData), void* userData) {
	ASSERT(callback);
	struct sp_port **ports;
	int n = 0;
	sp_list_ports(&ports);
	for (n = 0; ports[n]; n++) {
		const char* port_full_name = sp_get_port_name(ports[n]);
		callback(port_full_name, userData);
	}
	sp_free_port_list(ports);
	return n;
}
