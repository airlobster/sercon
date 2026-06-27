#include <stdlib.h>
#include "utils.h"
#include "r_array.h"

#define PAGE_SIZE (16)

/**
 * @brief Internal structure representing a dynamic array.
 */
typedef struct r_array_internal_t {
	/**< Pointer to the array elements */
	void** elements;
	/**< Number of elements in the array */
	size_t size;
	/**< Capacity of the array */
	size_t capacity;
	/**< Destructor function for array elements */
	r_array_dtor_t dtor;
} r_array_internal_t;

/**
 * @brief Default destructor function that does nothing.
 * @param element Pointer to the element (unused).
 */
static void null_dtor(void* element) {
	(void)element;
}

/**
 * @brief Create a new dynamic array.
 * @param dtor Destructor function for array elements.
 * @return r_array_t The created dynamic array.
 */
r_array_t r_array_create(r_array_dtor_t dtor) {
	r_array_internal_t* array = (r_array_internal_t*)malloc(sizeof(r_array_internal_t));
	if( ! array ) {
		DEBUG_MSG("ERROR: Failed to allocate memory for r_array_internal_t");
		return 0;
	}
	array->capacity = 0;
	array->size = 0;
	array->dtor = dtor ? dtor : null_dtor;
	return (r_array_t)array;
}

/**
 * @brief Destroy a dynamic array.
 * @param array The dynamic array to destroy.
 */
void r_array_destroy(r_array_t array) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	if( a) {
		if( a->elements ) {
			for( size_t i = 0; i < a->size; ++i ) {
				a->dtor(a->elements[i]);
			}
			free(a->elements);
		}
		free(a);
	}
}

/**
 * @brief Add an element to the dynamic array.
 * @param array The dynamic array.
 * @param element The element to add.
 */
void r_array_add(r_array_t array, void* element) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	if( a->size >= a->capacity ) {
			size_t new_capacity = a->capacity ? a->capacity * 2 : PAGE_SIZE;
			void** new_elements = realloc(a->elements, (new_capacity + 1) * sizeof(void*));
			if( ! new_elements ) {
				DEBUG_MSG("ERROR: Failed to allocate memory for r_array elements");
				return;
			};
			a->elements = new_elements;
			a->capacity = new_capacity;
	}
	a->elements[a->size++] = element;
	a->elements[a->size] = 0; // Null-terminate the array
}

/**
 * @brief Get an element from the dynamic array.
 * @param array The dynamic array.
 * @param index The index of the element to retrieve.
 * @return void* The element at the specified index.
 */
void* r_array_get(r_array_t array, size_t index) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	ASSERT(index < a->size);
	if( index >= a->size ) {
		DEBUG_MSG("ERROR: Index out of bounds in r_array_get");
		return 0;
	}
	return a->elements[index];
}

/**
 * @brief Get the number of elements in the dynamic array.
 * @param array The dynamic array.
 * @return size_t The number of elements in the array.
 */
size_t r_array_size(r_array_t array) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	return a->size;
}

/**
 * @brief Get the elements of the dynamic array.
 * @param array The dynamic array.
 * @return void** The elements of the array.
 */
void** r_array_elements(r_array_t array) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	return a->elements;
}

/**
 * @brief Detach the elements from the dynamic array, leaving it empty.
 * @param array The dynamic array.
 * @return void** The detached elements of the array.
 * @note The caller is responsible for freeing the detached elements and their contents.
 */
void** r_array_detach_elements(r_array_t array) {
	r_array_internal_t* a = (r_array_internal_t*)array;
	ASSERT(a);
	void** elements = a->elements;
	a->elements = 0;
	a->size = 0;
	a->capacity = 0;
	return elements;
}
