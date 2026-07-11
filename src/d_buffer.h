#ifndef _R_BUFFER_H
#define _R_BUFFER_H

/**
 * @file d_buffer.h
 * @brief Dynamic buffer implementation for managing variable-length data.
 *
 * This header defines the interface for a dynamic buffer that can grow as needed to accommodate data.
 * It provides functions to create, destroy, append data, retrieve data, and manage the buffer's size.
 *
 * The buffer is designed to be used in scenarios where the amount of data is not known in advance,
 * allowing for efficient memory management and dynamic resizing.
 *
 * @note The buffer is null-terminated after each append operation, making it suitable for string operations.
 *
 * @author Your Name
 * @date 2024-06-10
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _d_buffer_t* buffer_t;

buffer_t d_buffer_create(size_t max_size);
void d_buffer_destroy(buffer_t buffer);
size_t d_buffer_append_c(buffer_t buffer, char c);
size_t d_buffer_append(buffer_t buffer, const char* data, size_t length);
size_t d_buffer_append_s(buffer_t buffer, const char* data);
const char* d_buffer_get_data(buffer_t buffer);
char* d_buffer_detach_data(buffer_t buffer);
void d_buffer_clear(buffer_t buffer);
size_t d_buffer_size(buffer_t buffer);

#ifdef __cplusplus
}
#endif

#endif // _R_BUFFER_H
