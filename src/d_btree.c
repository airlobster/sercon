#include <stdlib.h>
#include "d_btree.h"
#include "utils.h"
#include "d_stack.h"

typedef struct _d_btree_node_t {
	/** The data stored in the node. */
	void* data;
	/** Pointer to the left child node. */
	struct _d_btree_node_t* left;
	/** Pointer to the right child node. */
	struct _d_btree_node_t* right;
} d_btree_node_t;

typedef struct _d_btree_state_t {
	/** The root node of the binary tree. */
	d_btree_node_t* root;
	/** The function to compare two nodes. */
	d_btree_compare_func_t compare_func;
	/** The function to free a node's data. */
	d_btree_free_func_t free_func;
	/** The number of nodes in the binary tree. */
	size_t size;
} d_btree_state_t;

/**
 * @brief Recursively destroys a binary tree node and its children.
 * @param bt The binary tree state.
 * @param node The node to destroy.
 */
static void d_btree_destroy_node(d_btree_state_t* bt, d_btree_node_t* node) {
	ASSERT(bt);
	if( ! node ) return;
	d_btree_destroy_node(bt, node->left);
	d_btree_destroy_node(bt, node->right);
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
static bool d_btree_add_node(d_btree_state_t* bt, d_btree_node_t** node, void* data) {
	ASSERT(bt);
	ASSERT(bt->compare_func);
	if( ! *node ) {
		*node = (d_btree_node_t*)malloc(sizeof(d_btree_node_t));
		if( ! *node ) {
			DEBUG_MSG("Failed to allocate memory for d_btree_node_t");
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
		return d_btree_add_node(bt, &(*node)->left, data);
	} else if( cmp > 0 ) {
		return d_btree_add_node(bt, &(*node)->right, data);
	}
	return false;
}

/**
 * @brief Recursively finds a node in the binary tree.
 * @param bt The binary tree state.
 * @param node The node to start the search from.
 * @param data The data to find.
 * @return The node containing the data, or NULL if not found.
 */
static d_btree_node_t* d_btree_find_node(d_btree_state_t* bt, d_btree_node_t* node, const void* data) {
	ASSERT(bt);
	ASSERT(bt->compare_func);
	if( ! node ) return NULL;
	int cmp = bt->compare_func(data, node->data);
	if( cmp == 0 ) {
		return node;
	} else if( cmp < 0 ) {
		return d_btree_find_node(bt, node->left, data);
	} else {
		return d_btree_find_node(bt, node->right, data);
	}
}

/**
 * @brief Creates a new binary tree.
 * @param compare_func The function to compare two nodes.
 * @param free_func The function to free a node's data.
 * @return A handle to the newly created binary tree.
 */
d_btree_t d_btree_create(d_btree_compare_func_t compare_func, d_btree_free_func_t free_func) {
	ASSERT(compare_func);
	d_btree_state_t* bt = (d_btree_state_t*)malloc(sizeof(d_btree_state_t));
	if( ! bt ) {
		DEBUG_MSG("Failed to allocate memory for d_btree_state_t");
		return NULL;
	}
	bt->root = NULL;
	bt->compare_func = compare_func;
	bt->free_func = free_func;
	bt->size = 0;
	return (d_btree_t)bt;
}

/**
 * @brief Destroys a binary tree.
 * @param tree The binary tree to destroy.
 */
void d_btree_destroy(d_btree_t tree) {
	ASSERT(tree);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	d_btree_destroy_node(bt, bt->root);
	free(bt);
}

/**
 * @brief Adds a node to the binary tree.
 * @param tree The binary tree.
 * @param data The data to add.
 * @return true if the node was added, false otherwise.
 */
bool d_btree_add(d_btree_t tree, void* data) {
	ASSERT(tree);
	ASSERT(data);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	return d_btree_add_node(bt, &bt->root, data);
}

/**
 * @brief Gets the size of the binary tree.
 * @param tree The binary tree.
 * @return The number of nodes in the binary tree.
 */
size_t d_btree_size(d_btree_t tree) {
	ASSERT(tree);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	return bt->size;
}

/**
 * @brief Resets the binary tree, removing all nodes.
 * @param tree The binary tree to reset.
 */
void d_btree_reset(d_btree_t tree) {
	ASSERT(tree);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	d_btree_destroy_node(bt, bt->root);
	bt->root = NULL;
	bt->size = 0;
}

/**
 * @brief Finds a node in the binary tree.
 * @param tree The binary tree.
 * @param data The data to find.
 * @return true if the data is found, false otherwise.
 */
bool d_btree_exists(d_btree_t tree, const void* data) {
	ASSERT(tree);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	d_btree_node_t* node = d_btree_find_node(bt, bt->root, data);
	return node != NULL;
}


/**
 * @brief internal b-tree iterator state structure
 */
typedef struct {
	/**< The binary tree being iterated over */
	d_btree_state_t* btree;
	/**< The stack used for in-order traversal */
	d_stack_t stack;
	/**< The current node in the traversal */
	d_btree_node_t* current;
} iterator_state_t;

/**
 * @brief Iterator function for in-order traversal of the binary tree.
 * @param state Pointer to the iterator state.
 * @param done Pointer to an integer indicating if the iteration is complete.
 * @param context User data passed to the iterator.
 * @return The next data element in the binary tree, or NULL if iteration is complete.
 */
static void* d_btree_next(void** state, int* done, void* context) {
	iterator_state_t* ctx = (iterator_state_t*)(*state);

	// first time initialization
	if( ! ctx ) {
		ctx = (iterator_state_t*)malloc(sizeof(iterator_state_t));
		if( ! ctx ) {
			DEBUG_MSG("Failed to allocate memory for iterator_state_t");
			*done = 1;
			return NULL;
		}
		ctx->btree = (d_btree_state_t*)context;
		ctx->current = ctx->btree->root;
		ctx->stack = d_stack_create(0, NULL);
		if( ! ctx->stack ) {
			DEBUG_MSG("Failed to create stack for iterator");
			free(ctx);
			*done = 1;
			return NULL;
		}

		*state = ctx;
	}

	while( ctx->current || ! d_stack_is_empty(ctx->stack) ) {
		if( ctx->current ) {
			d_stack_push(ctx->stack, ctx->current);
			ctx->current = ctx->current->left;
		} else {
			bool success;
			ctx->current = d_stack_pop(ctx->stack, &success);
			ASSERT(success);
			void* data = ctx->current->data;
			ctx->current = ctx->current->right;
			return data;
		}
	}

	*done = 1;
	return NULL;
}

/**
 * @brief Frees the resources associated with the binary tree iterator.
 * @param state Pointer to the iterator state.
 */
static void d_btree_iterator_free(void* state) {
	iterator_state_t* ctx = (iterator_state_t*)state;
	if( ctx ) {
		if( ctx->stack ) {
			d_stack_destroy(ctx->stack);
		}
		free(ctx);
	}
}

/**
 * @brief Initializes an iterator for in-order traversal of the binary tree.
 * @param tree The binary tree to iterate over.
 * @return An iterator for the binary tree.
 */
iterator_t d_btree_iterator(d_btree_t tree) {
	ASSERT(tree);
	d_btree_state_t* bt = (d_btree_state_t*)tree;
	return iterator_init(d_btree_next, d_btree_iterator_free, bt);
}
