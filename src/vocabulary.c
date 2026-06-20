#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "vocabulary.h"

#define ASSERT assert

/**
 * @brief A node in the vocabulary tree.
 */
typedef struct _vocabulary_node_t {
	struct _vocabulary_node_t *left, *right;
	char* word;
	size_t instances;
} vocabulary_node_t;

/**
 * @brief The internal structure of a vocabulary.
 */
typedef struct _vocabulary_internal_t {
	unsigned long options;
	vocabulary_node_t *root;
	size_t size;
	int (*cmp)(const char *s1, const char *s2);
	char** words_list;
	bool dirty;
} vocabulary_internal_t;

/**
 * @brief Recursively destroys a vocabulary node and its children.
 * @param node The node to destroy.
 */
static void destroy_node(vocabulary_node_t* node) {
	if( ! node ) return;
	destroy_node(node->left);
	destroy_node(node->right);
	ASSERT(node->word);
	free(node->word);
	free(node);
}

/**
 * @brief Recursively adds a word to the vocabulary tree.
 * @param vocab The vocabulary to add the word to.
 * @param node The current node in the tree.
 * @param word The word to add.
 * @return vocabulary_node_t* The newly added node, or NULL if the word already exists.
 */
static vocabulary_node_t* add_word_node(vocabulary_t vocab, vocabulary_node_t** node, const char* word) {
	ASSERT(vocab && vocab->cmp);
	ASSERT(word && *word);
	if( ! *node ) {
		*node = (vocabulary_node_t*)malloc(sizeof(vocabulary_node_t));
		if( ! *node ) return 0; // allocation failed
		(*node)->left = (*node)->right = 0;
		(*node)->word = strdup(word);
		(*node)->instances = 1;
		return *node;
	}
	int cmp = vocab->cmp(word, (*node)->word);
	if( cmp == 0 ) {
		(*node)->instances++;
		return 0; // word already exists
	}
	if( cmp < 0 ) {
		return add_word_node(vocab, &((*node)->left), word);
	}
	return add_word_node(vocab, &((*node)->right), word);
}

/**
 * @brief Recursively enumerates the words in the vocabulary tree.
 * @param vocab The vocabulary to enumerate.
 * @param node The current node in the tree.
 * @param callback The callback function to call for each word.
 * @param user_data User data to pass to the callback function.
 */
static int vocab_enum(
			vocabulary_t vocab,
			const vocabulary_node_t* node,
			int(*callback)(vocabulary_t, const char*, void*),
			void* user_data
		) {
	int ret;
	ASSERT(vocab);
	ASSERT(callback);
	if( ! node ) return 0;
	ASSERT(node->word && *node->word);
	if( (ret = vocab_enum(vocab, node->left, callback, user_data)) != 0 ) return ret;
	if( (ret = callback(vocab, node->word, user_data)) != 0 ) return ret;
	if( (ret = vocab_enum(vocab, node->right, callback, user_data)) != 0 ) return ret;
	return 0;
}

/**
 * @brief Creates a new vocabulary.
 * @param options Options for the vocabulary (e.g., case-insensitive).
 * @return vocabulary_t The newly created vocabulary.
 */
vocabulary_t vocab_create(unsigned long options) {
	vocabulary_internal_t* vocab = (vocabulary_internal_t*)malloc(sizeof(vocabulary_internal_t));
	ASSERT(vocab);
	if( ! vocab ) return 0; // allocation failed
	vocab->options = options;
	vocab->root = 0;
	vocab->size = 0;
	vocab->cmp = (options & VOCAB_OPT_CASE_INSENSITIVE) ? strcasecmp : strcmp;
	vocab->dirty = true;
	vocab->words_list = 0;
	return vocab;
}

/**
 * @brief Destroys a vocabulary.
 * @param vocab The vocabulary to destroy.
 */
void vocab_destroy(vocabulary_t vocab) {
	ASSERT(vocab);
	if( vocab->words_list ) {
		free(vocab->words_list);
	}
	destroy_node(vocab->root);
	free(vocab);
}

/**
 * @brief Adds a word to the vocabulary.
 * @param vocab The vocabulary to add the word to.
 * @param word The word to add.
 * @return bool True if the word was added, false if it already exists or if the input is invalid.
 * @note Assumes the word is a valid null-terminated trimmed string. Empty words are ignored.
 */
bool vocab_add_word(vocabulary_t vocab, const char* word) {
	ASSERT(vocab);
	ASSERT(word && *word);
	if( add_word_node(vocab, &vocab->root, word) ) {
		vocab->size++;
		vocab->dirty = true;
		return true;
	}
	return false;
}

/**
 * @brief Gets the size of the vocabulary.
 * @param vocab The vocabulary to get the size of.
 * @return size_t The number of words in the vocabulary.
 */
size_t vocab_size(vocabulary_t vocab) {
	ASSERT(vocab);
	return vocab->size;
}

/**
 * @brief Callback function to collect words into an array.
 * @param vocab The vocabulary the word belongs to.
 * @param word The current word.
 * @param user_data Pointer to the array of words.
 */
static int get_words_callback(vocabulary_t vocab, const char* word, void* user_data) {
	(void)vocab; // unused parameter
	char*** words_ptr = (char***)user_data;
	ASSERT(words_ptr && *words_ptr);
	ASSERT(word && *word);
	**words_ptr = (char*)word;
	(*words_ptr)++;
	return 0; // (continue)
}

/**
 * @brief Gets all the words in the vocabulary.
 * @param vocab The vocabulary to get the words from.
 * @return char** A null-terminated array of words.
 * @note The returned array is owned by the vocabulary and should not be freed by the caller.
 */
char** vocab_get_words(vocabulary_t vocab) {
	ASSERT(vocab);
	if( vocab->dirty ) {
		char** words = (char**)malloc(sizeof(char*) * (vocab->size+1));
		if( ! words ) return 0; // allocation failed
		// iterate tree and fill the array with the words
		char** words_ptr = words;
		vocab_enum(vocab, vocab->root, get_words_callback, &words_ptr);
		// null-terminate the array
		words[vocab->size] = 0;
		// refresh the cached words list
		if( vocab->words_list ) {
			free(vocab->words_list);
		}
		vocab->words_list = words;
		vocab->dirty = false;
	}
	return vocab->words_list;
}
