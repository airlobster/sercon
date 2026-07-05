#include <stdio.h>
#include <stdlib.h>
#include "iterator.h"

#ifndef NDEBUG
#	ifndef TRACE
#		define TRACE(fmt, ...) \
			fprintf(stderr, "[%s@%s:%d] ", __FUNCTION__, __FILE__, __LINE__); \
			fprintf(stderr, fmt, ##__VA_ARGS__);
#	endif // ASSERT
# ifndef ASSERT
#		define ASSERT(cond) \
			do { if(!(cond)) { TRACE("Assertion failed: %s\n", #cond); abort(); } } while(0)
#	endif // ASSERT
#else
#	ifndef TRACE
#		define TRACE(fmt, ...) (void)0
#	endif // ASSERT
# ifndef ASSERT
#		define ASSERT(cond) (void)0
#	endif // ASSERT
#endif // NDEBUG


typedef struct _iterator_context_t {
	/**< The state of the iterator. (created and managed by the next() function) */
	void* state;
	/**< The function to generate the next value. */
	iterator_next_func_t next;
	/**< The function to destroy the state of the iterator. */
	iterator_state_dtor_t dtor;
	/**< The context to be passed to the next() function. */
	void* context;
	/**< Indicates whether the iterator has reached the end. */
	int done;
	/**< The number of iterations performed by the iterator. */
	size_t n;
} iterator_context_t;

/**
 * @brief Initialize a new iterator.
 *
 * @param next The function to generate the next value.
 * @param dtor The function to destroy the state of the iterator (optional).
 * @param context The context to be passed to the next function (optional).
 * @return iterator_t The initialized iterator.
 */
iterator_t iterator_init(iterator_next_func_t next, iterator_state_dtor_t dtor, void* context) {
	ASSERT(next != NULL);
	iterator_t g = (iterator_t)malloc(sizeof(iterator_context_t));
	if( !g ) {
		TRACE("Failed to allocate memory for iterator\n");
		return NULL;
	}
	g->done = 0;
	g->state = NULL;
	g->next = next;
	g->dtor = dtor;
	g->context = context;
	g->n = 0;
	return g;
}

/**
 * @brief Free the resources associated with the iterator.
 *
 * @param g The iterator to be freed.
 * @details Invoked automatically by iterator_next() when the iterator is done.
 * Users should call it only when an iteration is aborted before the iterator is done.
 */
void iterator_free(iterator_t* g) {
	if( g && *g ) {
		if( (*g)->state && (*g)->dtor ) {
			(*g)->dtor((*g)->state);
		}
		free(*g);
		*g = NULL;
	}
}

/**
 * @brief Get the next value from the iterator.
 *
 * @param g Pointer to the iterator.
 * @return iterator_result_t The result of the next value.
 */
iterator_result_t iterator_next(iterator_t* g) {
	ASSERT(g != NULL && *g != NULL);
	void* value = (*g)->next(&(*g)->state, &(*g)->done, (*g)->context);
	iterator_result_t ret = (iterator_result_t){.value = value, .done = (*g)->done};
	++(*g)->n;
	if( (*g)->done ) {
		// TRACE("** Iterator finished after returning %zu values\n", (*g)->n - 1);
		iterator_free(g);
	}
	return ret;
}
