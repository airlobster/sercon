#ifndef _R_STACK_H
#define _R_STACK_H

/**
 * @file r_stack.h
 * @brief Header file for the r_stack library.
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
typedef struct r_stack_internal_t* r_stack_t;

/**
 * @brief Function pointer type for a destructor function to free stack elements.
 * @param data Pointer to the data to be freed.
 */
typedef void(*r_stack_dtor_t)(void*);

r_stack_t r_stack_create(size_t max_capacity, r_stack_dtor_t dtor);
void r_stack_destroy(r_stack_t stack);
bool r_stack_push(r_stack_t stack, void* data);
void* r_stack_pop(r_stack_t stack, bool* success);
void* r_stack_peek(r_stack_t stack, bool* success);
bool r_stack_is_empty(r_stack_t stack);
size_t r_stack_size(r_stack_t stack);

#ifdef __cplusplus
}
#endif

#endif // _R_STACK_H
