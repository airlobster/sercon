#include <sys/param.h>
#include <string.h>
#include "r_buffer.h"
#include "utils.h"

typedef struct _r_buffer_t {
	char* data;
	size_t length;
	size_t capacity;
} r_buffer_t;

buffer_t r_buffer_create() {
	r_buffer_t* b = (r_buffer_t*)malloc(sizeof(struct _r_buffer_t));
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	return b;
}

void r_buffer_destroy(buffer_t buffer) {
	r_buffer_t* b = (r_buffer_t*)buffer;
	ASSERT(b);
	if( b->data ) {
		free(b->data);
		b->data = NULL;
	}
	free(b);
}

void r_buffer_append(buffer_t buffer, const char* data, size_t length) {
	r_buffer_t* b = (r_buffer_t*)buffer;
	ASSERT(b);
	if( b->length + length >= b->capacity ) {
		size_t newCapacity = MAX(b->capacity * 2, b->length + length);
		char* newData = (char*)realloc(b->data, newCapacity + 1);
		if( ! newData ) {
			DEBUG_MSG("Failed to allocate memory for r_buffer");
			return;
		}
		b->data = newData;
		b->capacity = newCapacity;
	}
	strncpy(b->data + b->length, data, length);
	b->length += length;
	b->data[b->length] = '\0'; // null-terminate the buffer
}

const char* r_buffer_get_data(buffer_t buffer) {
	r_buffer_t* b = (r_buffer_t*)buffer;
	ASSERT(b);
	return b->data;
}

char* r_buffer_detach_data(buffer_t buffer) {
	r_buffer_t* b = (r_buffer_t*)buffer;
	ASSERT(b);
	char* data = b->data;
	b->data = NULL;
	b->length = 0;
	b->capacity = 0;
	return data;
}
