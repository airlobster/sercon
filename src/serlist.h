#ifndef _SERLIST_H_
#define _SERLIST_H_

/**
 * @file serlist.h
 * @brief Header file for enumerating serial ports.
 */

#include "r_array.h"

#ifdef __cplusplus
extern "C" {
#endif

r_array_t enumSerialPorts();

#ifdef __cplusplus
}
#endif

#endif /* _SERLIST_H_ */
