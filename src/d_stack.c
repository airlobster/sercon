#include <stdlib.h>
#include "d_stack.h"
#include "utils.h"

/**
 * @brief Internal structure representing a node in the stack.
 */
typedef struct _d_stack_node_t {
		/**< The data stored in the stack node */
		void* data;
		/**< Pointer to the next node in the stack */
		struct _d_stack_node_t* next;
} d_stack_node_t;

/**
 * @brief Internal structure representing the stack.
 */
typedef struct _d_stack_internal_t {
		/**< Pointer to the top node in the stack */
		d_stack_node_t* top;
		/**< The current size of the stack */
		size_t size;
		/**< The maximum capacity of the stack */
		size_t max_capacity;
		/**< Destructor function for stack elements */
		d_stack_dtor_t dtor;
} d_stack_internal_t;

/**
 * @brief Creates a new stack with the specified maximum capacity and destructor function.
 * @param max_capacity The maximum number of elements the stack can hold (0 for unlimited).
 * @param dtor The destructor function to free stack elements (can be NULL).
 * @return A pointer to the newly created stack, or NULL on failure.
 */
d_stack_t d_stack_create(size_t max_capacity, d_stack_dtor_t dtor) {
	d_stack_internal_t* stack = (d_stack_internal_t*)malloc(sizeof(d_stack_internal_t));
	if( ! stack ) {
		DEBUG_MSG("Failed to allocate memory for stack");
		return NULL;
	}
	stack->top = NULL;
	stack->size = 0;
	stack->max_capacity = max_capacity;
	stack->dtor = dtor;
	return (d_stack_t)stack;
}

/**
 * @brief Destroys the stack and frees all associated memory.
 * @param stack The stack to destroy.
 */
void d_stack_destroy(d_stack_t stack) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	d_stack_node_t* current = s->top;
	while( current ) {
		d_stack_node_t* next = current->next;
		if( s->dtor ) {
			s->dtor(current->data);
		}
		free(current);
		current = next;
	}
	free(s);
}

/**
 * @brief Pushes an element onto the stack.
 * @param stack The stack to push the element onto.
 * @param data The data to push onto the stack.
 * @return true if the element was successfully pushed, false if the stack is full or on error.
 */
bool d_stack_push(d_stack_t stack, void* data) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	if( s->max_capacity && s->size >= s->max_capacity ) {
		DEBUG_MSG("Stack is full, cannot push element");
		return false;
	}
	d_stack_node_t* new_node = (d_stack_node_t*)malloc(sizeof(d_stack_node_t));
	if( ! new_node ) {
		DEBUG_MSG("Failed to allocate memory for new stack node");
		return false;
	}
	new_node->data = data;
	new_node->next = s->top;
	s->top = new_node;
	s->size++;
	return true;
}

/**
 * @brief Pops an element from the stack.
 * @param stack The stack to pop the element from.
 * @param success Pointer to a boolean that will be set to true if the pop was successful, false otherwise.
 * @return The data popped from the stack, or NULL if the stack is empty.
 */
void* d_stack_pop(d_stack_t stack, bool* success) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	if( s->size == 0 ) {
		DEBUG_MSG("Stack is empty, cannot pop element");
		if( success ) *success = false;
		return NULL;
	}
	d_stack_node_t* top_node = s->top;
	void* data = top_node->data;
	s->top = top_node->next;
	s->size--;
	if( success ) *success = true;
	free(top_node);
	return data;
}

/**
 * @brief Peeks at the top element of the stack without removing it.
 * @param stack The stack to peek at.
 * @param success Pointer to a boolean that will be set to true if the peek was successful, false otherwise.
 * @return The data at the top of the stack, or NULL if the stack is empty.
 */
void* d_stack_peek(d_stack_t stack, bool* success) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	if( s->size == 0 ) {
		DEBUG_MSG("Stack is empty, cannot peek element");
		if( success ) *success = false;
		return NULL;
	}
	if( success ) *success = true;
	return s->top->data;
}

/**
 * @brief Checks if the stack is empty.
 * @param stack The stack to check.
 * @return true if the stack is empty, false otherwise.
 */
bool d_stack_is_empty(d_stack_t stack) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	return s->size == 0;
}

/**
 * @brief Gets the current size of the stack.
 * @param stack The stack to check.
 * @return The number of elements in the stack.
 */
size_t d_stack_size(d_stack_t stack) {
	ASSERT(stack);
	d_stack_internal_t* s = (d_stack_internal_t*)stack;
	return s->size;
}
