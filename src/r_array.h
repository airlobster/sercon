#ifndef _R_ARRAY_H
#define _R_ARRAY_H

/**
 * @file r_array.h
 * @brief Header file for the dynamic array implementation.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque type representing a dynamic array.
 */
typedef struct r_array_internal_t* r_array_t;

/**
 * @brief Function pointer type for element destructor.
 * @param element Pointer to the element to be destroyed.
 */
typedef void(*r_array_dtor_t)(void* element);

r_array_t r_array_create(size_t maxEntries, r_array_dtor_t dtor);
void r_array_destroy(r_array_t array);
void r_array_add(r_array_t array, void* element);
void* r_array_get(r_array_t array, size_t index);
size_t r_array_size(r_array_t array);
void** r_array_elements(r_array_t array);
void** r_array_detach_elements(r_array_t array);

#ifdef __cplusplus
}
#endif

#endif // _R_ARRAY_H
