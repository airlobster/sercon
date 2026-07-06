#ifndef _R_ARRAY_H
#define _R_ARRAY_H

/**
 * @file d_array.h
 * @brief Header file for the dynamic array implementation.
 */

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque type representing a dynamic array.
 */
typedef struct d_array_internal_t* d_array_t;

/**
 * @brief Function pointer type for element destructor.
 * @param element Pointer to the element to be destroyed.
 */
typedef void(*d_array_dtor_t)(void* element);

d_array_t d_array_create(size_t maxEntries, d_array_dtor_t dtor);
void d_array_destroy(d_array_t array);
bool d_array_add(d_array_t array, void* element);
void* d_array_get(d_array_t array, size_t index);
bool d_array_remove(d_array_t array, size_t index);
size_t d_array_size(d_array_t array);
const void* const* d_array_elements(d_array_t array);
void** d_array_detach_elements(d_array_t array);

#ifdef __cplusplus
}
#endif

#endif // _R_ARRAY_H
