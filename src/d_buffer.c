#include <sys/param.h>
#include <string.h>
#include "d_buffer.h"
#include "utils.h"
#include "mem.h"

#define DEFAULT_MAX_SIZE (512)

typedef struct _d_buffer_t {
	char* data;
	size_t length;
	size_t capacity;
	size_t max_size;
} d_buffer_t;

buffer_t d_buffer_create(size_t max_size) {
	d_buffer_t* b = (d_buffer_t*)MALLOC(sizeof(struct _d_buffer_t));
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	b->max_size = max_size ? max_size : DEFAULT_MAX_SIZE;
	return b;
}

void d_buffer_destroy(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	if( b->data ) {
		FREE(b->data);
		b->data = NULL;
	}
	FREE(b);
}

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

const char* d_buffer_get_data(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	return b->data;
}

char* d_buffer_detach_data(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	char* data = b->data;
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	return data;
}

void d_buffer_clear(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	// reuse current allocated memory if possible
	if( b->data ) {
		b->data[0] = '\0'; // null-terminate the buffer
	}
	b->length = 0;
}

size_t d_buffer_size(buffer_t buffer) {
	d_buffer_t* b = (d_buffer_t*)buffer;
	ASSERT(b);
	return b->length;
}
