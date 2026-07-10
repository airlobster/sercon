#include <sys/param.h>
#include <string.h>
#include "d_buffer.h"
#include "utils.h"

#define DEFAULT_MAX_SIZE (512)

typedef struct _d_buffer_t {
	/** Dynamic buffer data. */
	char* data;
	/** Current length of the data in the buffer. */
	size_t length;
	/** Current capacity of the buffer. */
	size_t capacity;
	/** Maximum allowed size of the buffer. */
	size_t max_size;
} d_buffer_t;

/**
 * @brief Create a new dynamic buffer.
 * @param max_size The maximum allowed size of the buffer. If 0, defaults to DEFAULT_MAX_SIZE.
 * @return buffer_t A new dynamic buffer instance.
 */
buffer_t d_buffer_create(size_t max_size) {
	d_buffer_t* b = (d_buffer_t*)MALLOC(sizeof(struct _d_buffer_t));
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	b->max_size = max_size ? max_size : DEFAULT_MAX_SIZE;
	return b;
}

/**
 * @brief Destroy a dynamic buffer and free its resources.
 * @param buffer The dynamic buffer to destroy.
 */
void d_buffer_destroy(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	if( b->data ) {
		FREE(b->data);
		b->data = NULL;
	}
	FREE(b);
}

/**
 * @brief Append data to a dynamic buffer.
 * @param buffer The dynamic buffer.
 * @param data The data to append.
 * @param length The length of the data to append.
 * @return size_t The number of bytes appended, or 0 on failure.
 */
size_t d_buffer_append(buffer_t buffer, const char* data, size_t length) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	if( b->max_size && b->length + length >= b->max_size ) {
		DEBUG_MSG("Buffer overflow: cannot append data to buffer");
		return 0;
	}
	if( b->length + length >= b->capacity ) {
		size_t newCapacity = MAX(b->capacity * 2, b->length + length);
		char* newData = (char*)REALLOC(b->data, (newCapacity + 1) * sizeof(char)); // +1 for null terminator
		if( ! newData ) {
			return 0;
		}
		b->data = newData;
		b->capacity = newCapacity;
	}
	memcpy(b->data + b->length, data, length);
	b->length += length;
	b->data[b->length] = '\0'; // null-terminate the buffer
	return length;
}

/**
 * @brief Append a null-terminated string to a dynamic buffer.
 * @param buffer The dynamic buffer.
 * @param data The null-terminated string to append.
 * @return size_t The number of bytes appended, or 0 on failure.
 */
size_t d_buffer_append_s(buffer_t buffer, const char* data) {
	ASSERT(buffer);
	return d_buffer_append(buffer, data, strlen(data));
}

/**
 * @brief Get the data from a dynamic buffer.
 * @param buffer The dynamic buffer.
 * @return const char* The data in the buffer.
 */
const char* d_buffer_get_data(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	return b->data;
}

/**
 * @brief Detach the data from a dynamic buffer.
 * @param buffer The dynamic buffer.
 * @return char* The detached data. The caller is responsible for freeing it.
 */
char* d_buffer_detach_data(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	char* data = b->data;
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	return data;
}
/**
 * @brief Clear the contents of a dynamic buffer.
 * @param buffer The dynamic buffer.
 */
void d_buffer_clear(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	// reuse current allocated memory if possible
	if( b->data ) {
		b->data[0] = '\0'; // null-terminate the buffer
	}
	b->length = 0;
}

/**
 * @brief Get the current size of a dynamic buffer.
 * @param buffer The dynamic buffer.
 * @return size_t The current size of the buffer.
 */
size_t d_buffer_size(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	return b->length;
}
