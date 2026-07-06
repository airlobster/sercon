#include <stdlib.h>
#include "utils.h"
#include "d_array.h"

#define PAGE_SIZE (16)
#define DEFAULT_MAX_ENTRIES (1024*2)

/**
 * @brief Internal structure representing a dynamic array.
 */
typedef struct {
	void** elements; /**< Pointer to the array elements */
	size_t size; /**< Number of elements in the array */
	size_t capacity; /**< Capacity of the array */
	size_t maxEntries; /**< Maximum number of entries allowed in the array */
	d_array_dtor_t dtor; /**< Destructor function for array elements */
} d_array_internal_t;

/**
 * @brief Default destructor function that does nothing.
 * @param element Pointer to the element (unused).
 */
static void null_dtor(void* element) {
	(void)element;
}

/**
 * @brief Create a new dynamic array.
 * @param maxEntries The initial maximum number of entries.
 * @param dtor Destructor function for array elements (optional).
 * @return d_array_t The created dynamic array.
 */
d_array_t d_array_create(size_t maxEntries, d_array_dtor_t dtor) {
	d_array_internal_t* a = (d_array_internal_t*)malloc(sizeof(d_array_internal_t));
	if( ! a ) {
		DEBUG_MSG("ERROR: Failed to allocate memory for d_array_internal_t");
		return 0;
	}
	a->elements = 0;
	a->capacity = 0;
	a->size = 0;
	a->maxEntries = maxEntries ? maxEntries : DEFAULT_MAX_ENTRIES;
	a->dtor = dtor ? dtor : null_dtor;

	return (d_array_t)a;
}

/**
 * @brief Destroy a dynamic array.
 * @param array The dynamic array to destroy.
 */
void d_array_destroy(d_array_t array) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	if( a->elements ) {
		for( size_t i = 0; i < a->size; ++i ) {
			ASSERT(a->dtor);
			a->dtor(a->elements[i]);
		}
		free(a->elements);
	}
	free(a);
}

/**
 * @brief Add an element to the dynamic array.
 * @param array The dynamic array.
 * @param element The element to add.
 * @return bool True if the element was added successfully, false otherwise.
 */
bool d_array_add(d_array_t array, void* element) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	ASSERT(element);
	if( ! element ) return false;
	// limit reached?
	if( a->maxEntries && a->size >= a->maxEntries ) {
		DEBUG_MSG("ERROR: Maximum number of entries reached in d_array_add");
		return false;
	}
	// need to grow the array?
	if( a->size >= a->capacity ) {
		size_t new_capacity = a->capacity ? a->capacity * 2 : PAGE_SIZE;
		void** new_elements = realloc(a->elements, (new_capacity + 1) * sizeof(void*));
		if( ! new_elements ) {
			DEBUG_MSG("ERROR: Failed to allocate memory for d_array elements");
			return false;
		};
		a->elements = new_elements;
		a->capacity = new_capacity;
	}
	a->elements[a->size++] = element;
	a->elements[a->size] = 0; // Null-terminate the array
	return true;
}

/**
 * @brief Get an element from the dynamic array.
 * @param array The dynamic array.
 * @param index The index of the element to retrieve.
 * @return void* The element at the specified index.
 */
void* d_array_get(d_array_t array, size_t index) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	ASSERT(index < a->size);
	if( index >= a->size ) {
		DEBUG_MSG("ERROR: Index out of bounds in d_array_get");
		return 0;
	}
	return a->elements[index];
}

/**
 * @brief Remove an element from the dynamic array.
 * @param array The dynamic array.
 * @param index The index of the element to remove.
 * @return bool True if the element was removed successfully, false otherwise.
 */
bool d_array_remove(d_array_t array, size_t index) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	ASSERT(index < a->size);
	if( index >= a->size ) {
		DEBUG_MSG("ERROR: Index out of bounds in d_array_remove");
		return false;
	}
	if( a->dtor ) {
		a->dtor(a->elements[index]);
	}
	for( size_t i = index; i < a->size - 1; ++i ) {
		a->elements[i] = a->elements[i + 1];
	}
	a->elements[--a->size] = 0; // Null-terminate the array
	return true;
}

/**
 * @brief Get the number of elements in the dynamic array.
 * @param array The dynamic array.
 * @return size_t The number of elements in the array.
 */
size_t d_array_size(d_array_t array) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	return a->size;
}

/**
 * @brief Get the elements of the dynamic array.
 * @param array The dynamic array.
 * @return const void* const* The elements of the array.
 */
const void* const*  d_array_elements(d_array_t array) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	return (const void* const*)a->elements;
}

/**
 * @brief Detach the elements from the dynamic array, leaving it empty.
 * @param array The dynamic array.
 * @return void** The detached elements of the array.
 * @note d_array will no longer manage the memory of the detached elements;
 */
void** d_array_detach_elements(d_array_t array) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	void** elements = a->elements;
	a->elements = 0;
	a->size = 0;
	a->capacity = 0;
	return elements;
}

/**
 * @brief Clear the dynamic array, destroying all elements.
 * @param array The dynamic array.
 */
void d_array_clear(d_array_t array) {
	d_array_internal_t* a = (d_array_internal_t*)array;
	ASSERT(a);
	for( size_t i = 0; i < a->size; ++i ) {
		ASSERT(a->dtor);
		a->dtor(a->elements[i]);
	}
	a->size = 0;
}

/**
 * @brief Populate the dynamic array from an iterator.
 * @param array The dynamic array.
 * @param iter The iterator to populate from.
 * @return size_t The number of elements added to the array.
 */
size_t d_array_from_iterator(d_array_t array, iterator_t iter) {
	ASSERT(array);
	ASSERT(iter);
	d_array_clear(array);
	_foreach(iter, r) {
		d_array_add(array, r.value);
	}
	return d_array_size(array);
}
