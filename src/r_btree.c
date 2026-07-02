#include <stdlib.h>
#include "r_btree.h"
#include "utils.h"

typedef struct _r_btree_node_t {
		void* data;
		struct _r_btree_node_t* left;
		struct _r_btree_node_t* right;
} r_btree_node_t;

typedef struct _r_btree_state_t {
		r_btree_node_t* root;
		r_btree_compare_func_t compare_func;
		r_btree_free_func_t free_func;
		size_t size;
} r_btree_state_t;

/**
 * @brief Recursively destroys a binary tree node and its children.
 * @param bt The binary tree state.
 * @param node The node to destroy.
 */
static void r_btree_destroy_node(r_btree_state_t* bt, r_btree_node_t* node) {
	ASSERT(bt);
	if( ! node ) return;
	r_btree_destroy_node(bt, node->left);
	r_btree_destroy_node(bt, node->right);
	if( bt->free_func && node->data ) {
		bt->free_func(node->data);
	}
	free(node);
}

/**
 * @brief Recursively adds a node to the binary tree.
 * @param bt The binary tree state.
 * @param node The node to add to.
 * @param data The data to add.
 * @return true if the node was added, false otherwise.
 */
static bool r_btree_add_node(r_btree_state_t* bt, r_btree_node_t** node, void* data) {
	ASSERT(bt);
	ASSERT(bt->compare_func);
	if( ! *node ) {
		*node = (r_btree_node_t*)malloc(sizeof(r_btree_node_t));
		if( ! *node ) {
			DEBUG_MSG("Failed to allocate memory for r_btree_node_t");
			return false;
		}
		(*node)->data = data;
		(*node)->left = NULL;
		(*node)->right = NULL;
		bt->size++;
		return true;
	}
	int cmp = bt->compare_func(data, (*node)->data);
	if( cmp < 0 ) {
		return r_btree_add_node(bt, &(*node)->left, data);
	} else if( cmp > 0 ) {
		return r_btree_add_node(bt, &(*node)->right, data);
	}
	return false;
}

/**
 * @brief Recursively traverses a binary tree node and its children in-order.
 * @param bt The binary tree state.
 * @param node The node to traverse.
 * @param traverse_func The function to call for each node's data.
 * @param context User data to pass to the traverse function.
 */
static void r_btree_traverse_node(r_btree_state_t* bt, r_btree_node_t* node, r_btree_traverse_func_t traverse_func, void* context) {
	ASSERT(bt);
	if( ! node ) return;
	r_btree_traverse_node(bt, node->left, traverse_func, context);
	traverse_func(node->data, context);
	r_btree_traverse_node(bt, node->right, traverse_func, context);
}

/**
 * @brief Recursively finds a node in the binary tree.
 * @param bt The binary tree state.
 * @param node The node to start the search from.
 * @param data The data to find.
 * @return The node containing the data, or NULL if not found.
 */
static r_btree_node_t* r_btree_find_node(r_btree_state_t* bt, r_btree_node_t* node, const void* data) {
	ASSERT(bt);
	if( ! node ) return NULL;
	int cmp = bt->compare_func(data, node->data);
	if( cmp == 0 ) {
		return node;
	} else if( cmp < 0 ) {
		return r_btree_find_node(bt, node->left, data);
	} else {
		return r_btree_find_node(bt, node->right, data);
	}
}

/**
 * @brief Creates a new binary tree.
 * @param compare_func The function to compare two nodes.
 * @param free_func The function to free a node's data.
 * @return A handle to the newly created binary tree.
 */
r_btree_t r_btree_create(r_btree_compare_func_t compare_func, r_btree_free_func_t free_func) {
	ASSERT(compare_func);
	r_btree_state_t* bt = (r_btree_state_t*)malloc(sizeof(r_btree_state_t));
	if( ! bt ) {
		DEBUG_MSG("Failed to allocate memory for r_btree_state_t");
		return NULL;
	}
	bt->root = NULL;
	bt->compare_func = compare_func;
	bt->free_func = free_func;
	bt->size = 0;
	return (r_btree_t)bt;
}

/**
 * @brief Destroys a binary tree.
 * @param tree The binary tree to destroy.
 */
void r_btree_destroy(r_btree_t tree) {
	ASSERT(tree);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	r_btree_destroy_node(bt, bt->root);
	free(bt);
}

/**
 * @brief Adds a node to the binary tree.
 * @param tree The binary tree.
 * @param data The data to add.
 * @return true if the node was added, false otherwise.
 */
bool r_btree_add(r_btree_t tree, void* data) {
	ASSERT(tree);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	return r_btree_add_node(bt, &bt->root, data);
}

/**
 * @brief Gets the size of the binary tree.
 * @param tree The binary tree.
 * @return The number of nodes in the binary tree.
 */
size_t r_btree_size(r_btree_t tree) {
	ASSERT(tree);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	return bt->size;
}

/**
 * @brief Resets the binary tree, removing all nodes.
 * @param tree The binary tree to reset.
 */
void r_btree_reset(r_btree_t tree) {
	ASSERT(tree);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	r_btree_destroy_node(bt, bt->root);
	bt->root = NULL;
	bt->size = 0;
}

/**
 * @brief Traverses the binary tree in-order, calling a user-provided function for each node's data.
 * @param tree The binary tree to traverse.
 * @param traverse_func The function to call for each node's data.
 * @param context User data to pass to the traverse function.
 */
void r_btree_traverse(r_btree_t tree, r_btree_traverse_func_t traverse_func, void* context) {
	ASSERT(tree);
	ASSERT(traverse_func);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	r_btree_traverse_node(bt, bt->root, traverse_func, context);
}

/**
 * @brief Finds a node in the binary tree.
 * @param tree The binary tree.
 * @param data The data to find.
 * @return true if the data is found, false otherwise.
 */
bool r_btree_exists(r_btree_t tree, const void* data) {
	ASSERT(tree);
	r_btree_state_t* bt = (r_btree_state_t*)tree;
	r_btree_node_t* node = r_btree_find_node(bt, bt->root, data);
	return node != NULL;
}
