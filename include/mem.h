#ifndef _MEM_H
#define _MEM_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NDEBUG

#	define MALLOC(size) malloc(size)
#	define REALLOC(ptr, size) realloc(ptr, size)
#	define FREE(ptr) free(ptr)
#	define STRDUP(str) strdup(str)
#	define STRNDUP(str, size) strndup(str, size)

#else // NDEBUG

#	define MALLOC(size) mem_alloc(size, __FILE__, __LINE__, __FUNCTION__)
#	define REALLOC(ptr, size) mem_realloc(ptr, size, __FILE__, __LINE__, __FUNCTION__)
#	define FREE(ptr) mem_free(ptr, __FILE__, __LINE__, __FUNCTION__)
#	define STRDUP(str) mem_strdup(str, __FILE__, __LINE__, __FUNCTION__)
#	define STRNDUP(str, size) mem_strndup(str, size, __FILE__, __LINE__, __FUNCTION__)
void* mem_alloc(size_t size, const char* file, int line, const char* func);
void* mem_realloc(void* ptr, size_t size, const char* file, int line, const char* func);
void mem_free(void* ptr, const char* file, int line, const char* func);
char* mem_strdup(const char* str, const char* file, int line, const char* func);
char* mem_strndup(const char* str, size_t size, const char* file, int line, const char* func);

#endif // NDEBUG


#ifdef __cplusplus
}
#endif

#endif // _MEM_H
