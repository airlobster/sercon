#ifndef _SERLIST_H_
#define _SERLIST_H_

/**
 * @file serlist.h
 * @brief Header file for enumerating serial ports.
 */
 
#ifdef __cplusplus
extern "C" {
#endif

int enumSerialPorts(void(*callback)(const char* port, void* userData), void* userData);

#ifdef __cplusplus
}
#endif

#endif /* _SERLIST_H_ */
