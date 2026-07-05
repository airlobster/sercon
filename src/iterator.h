#ifndef _ITERATOR_H_
#define _ITERATOR_H_

/**
 * @file iterator.h
 * @brief A simple iterator implementation in C.
 *
 * This header defines the structures and functions for creating and using iterators in C.
 * An iterator allows sequential access to elements in a collection without exposing the underlying representation.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque type representing an iterator.
 */
typedef struct _iterator_context_t* iterator_t;

/**
 * @brief Structure representing the result of an iterator operation.
 */
typedef struct _iterator_result_t {
	/**< Indicates whether the iterator has reached the end. */
	int done;
	/**< The value returned by the iterator. */
	void* value;
} iterator_result_t;

/**
 * @brief Function pointer type for retrieving the next element from an iterator.
 * @param state Pointer to the state to be passed to the next function.
 * @param done Pointer to an integer that will be set to 1 if the iterator has reached the end, 0 otherwise.
 * @param context Pointer to a user-defined context that will be passed to the next function.
 * @return The next element from the iterator.
 * @details At first, the state points to a NULL pointer. This is when it should be allocated and initialized
 * appropriately. The next function is responsible for updating the state and setting the done flag when the
 * iteration is complete.
 */
typedef void* (*iterator_next_func_t)(void** state, int* done, void* context);

/**
 * @brief Function pointer type for freeing the state of an iterator.
 * @param state The state to be freed.
 */
typedef void(*iterator_state_dtor_t)(void* state);

iterator_t iterator_init(iterator_next_func_t next, iterator_state_dtor_t dtor, void* context);
void iterator_free(iterator_t* g);
iterator_result_t iterator_next(iterator_t* g);


#define _foreach(it, result) \
	for(iterator_result_t result = iterator_next(&it); it && ! result.done; result = iterator_next(&it))

#ifdef __cplusplus
}
#endif

#endif // _ITERATOR_H_
