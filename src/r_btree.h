#ifndef _R_BTREE_H
#define _R_BTREE_H

/**
 * @file r_btree.h
 * @brief Header file for the r_btree library.
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

#ifdef __cplusplus
extern "C" {
#endif

typedef void* r_btree_t;

typedef int (*r_btree_compare_func_t)(const void* a, const void* b);
typedef void (*r_btree_free_func_t)(void* data);
typedef void (*r_btree_traverse_func_t)(void* data, void* context);

r_btree_t r_btree_create(r_btree_compare_func_t compare_func, r_btree_free_func_t free_func);
void r_btree_destroy(r_btree_t tree);
bool r_btree_add(r_btree_t tree, void* data);
bool r_btree_exists(r_btree_t tree, const void* data);
size_t r_btree_size(r_btree_t tree);
void r_btree_reset(r_btree_t tree);
void r_btree_traverse(r_btree_t tree, r_btree_traverse_func_t traverse_func, void* context);

#ifdef __cplusplus
}
#endif

#endif // _R_BTREE_H
