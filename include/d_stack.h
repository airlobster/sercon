#ifndef _R_STACK_H
#define _R_STACK_H

/**
 * @file d_stack.h
 * @brief Header file for the d_stack library.
 *
 * This library provides a simple interface for creating and managing stacks.
 * It allows for pushing and popping elements, checking if the stack is empty,
 * and retrieving the size of the stack.
 *
 * The stack is implemented as a linked list, allowing for efficient memory usage
 * and dynamic resizing when needed.
 *
 * @note This library is designed to be used in C programs and provides a C-style API.
 */

 #include <stdbool.h>
 #include <stddef.h>

 #ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque type representing a stack.
 */
typedef struct d_stack_internal_t* d_stack_t;

/**
 * @brief Function pointer type for a destructor function to free stack elements.
 * @param data Pointer to the data to be freed.
 */
typedef void(*d_stack_dtor_t)(void*);

d_stack_t d_stack_create(size_t max_capacity, d_stack_dtor_t dtor);
void d_stack_destroy(d_stack_t stack);
bool d_stack_push(d_stack_t stack, void* data);
void* d_stack_pop(d_stack_t stack, bool* success);
void* d_stack_peek(d_stack_t stack, bool* success);
bool d_stack_is_empty(d_stack_t stack);
size_t d_stack_size(d_stack_t stack);

#ifdef __cplusplus
}
#endif

#endif // _R_STACK_H
