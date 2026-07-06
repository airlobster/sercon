#ifndef _R_BUFFER_H
#define _R_BUFFER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _d_buffer_t* buffer_t;

buffer_t d_buffer_create(size_t max_size);
void d_buffer_destroy(buffer_t buffer);
size_t d_buffer_append(buffer_t buffer, const char* data, size_t length);
const char* d_buffer_get_data(buffer_t buffer);
char* d_buffer_detach_data(buffer_t buffer);
void d_buffer_clear(buffer_t buffer);
size_t d_buffer_size(buffer_t buffer);

#ifdef __cplusplus
}
#endif

#endif // _R_BUFFER_H
