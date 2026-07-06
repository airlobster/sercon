#include <stdlib.h>
#include <string.h>
#include "mem.h"
#include "utils.h"

#ifndef NDEBUG

void* mem_alloc(size_t size, const char* file, int line, const char* func) {
	void* ptr = malloc(size);
	if( ! ptr ) {
		debug_msg(file, line, func, "Memory allocation failed\n");
	}
	return ptr;
}

void* mem_realloc(void* ptr, size_t size, const char* file, int line, const char* func) {
	void* new_ptr = realloc(ptr, size);
	if( ! new_ptr ) {
		debug_msg(file, line, func, "Memory reallocation failed\n");
	}
	return new_ptr;
}

void mem_free(void* ptr, const char* file, int line, const char* func) {
	if( ! ptr ) {
		debug_msg(file, line, func, "Attempted to free a NULL pointer\n");
		return;
	}
	free(ptr);
}

char* mem_strdup(const char* str, const char* file, int line, const char* func) {
	if( ! str ) {
		debug_msg(file, line, func, "Attempted to duplicate a NULL string\n");
		return NULL;
	}
	char* new_str = strdup(str);
	if( ! new_str ) {
		debug_msg(file, line, func, "String duplication failed\n");
	}
	return new_str;
}

char* mem_strndup(const char* str, size_t size, const char* file, int line, const char* func) {
	if( ! str ) {
		debug_msg(file, line, func, "Attempted to duplicate a NULL string\n");
		return NULL;
	}
	char* new_str = strndup(str, size);
	if( ! new_str ) {
		debug_msg(file, line, func, "String duplication failed\n");
	}
	return new_str;
}

#endif
