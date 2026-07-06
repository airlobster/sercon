#ifndef _R_BTREE_H
#define _R_BTREE_H

/**
 * @file d_btree.h
 * @brief Header file for the d_btree library.
 *
 * This library provides a simple interface for creating and managing binary trees.
 * It allows for adding elements, traversing the tree, and performing various operations
 * on the tree structure.
 *
 * The binary tree is implemented as a balanced binary search tree, ensuring efficient
 * insertion, deletion, and lookup operations.
 *
 * @note This library is designed to be used in C programs and provides a C-style API.
 */

#include <stdbool.h>
#include <stddef.h>
#include "d_array.h"
#include "iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* d_btree_t;

typedef int (*d_btree_compare_func_t)(const void* a, const void* b);
typedef void (*d_btree_free_func_t)(void* data);
typedef void (*d_btree_traverse_func_t)(void* data, void* context);

d_btree_t d_btree_create(d_btree_compare_func_t compare_func, d_btree_free_func_t free_func);
void d_btree_destroy(d_btree_t tree);
bool d_btree_add(d_btree_t tree, void* data);
bool d_btree_exists(d_btree_t tree, const void* data);
size_t d_btree_size(d_btree_t tree);
void d_btree_reset(d_btree_t tree);

iterator_t d_btree_iterator(d_btree_t tree);

#ifdef __cplusplus
}
#endif

#endif // _R_BTREE_H
