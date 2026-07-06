#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "vocabulary.h"
#include "utils.h"
#include "d_btree.h"

/**
 * @brief The internal structure of a vocabulary.
 */
typedef struct _vocabulary_internal_t {
	unsigned long options; /**< Options for the vocabulary. */
	d_btree_t words_tree; /**< The binary tree for fast word lookup. */
	char** words_list; /**< The list of words in the vocabulary. */
} vocabulary_internal_t;

/**
 * @brief Destroys the words-list cache.
 * @param vocab The vocabulary whose words list is to be destroyed.
 */
static void destroy_words_list(vocabulary_internal_t* vocab) {
	ASSERT(vocab);
	if( ! vocab->words_list ) return;
	for(char** w = vocab->words_list; *w; w++) {
		free(*w);
	}
	free(vocab->words_list);
	vocab->words_list = NULL;
}

/**
 * @brief Creates a new vocabulary.
 * @param options Options for the vocabulary (e.g., case-insensitive).
 * @param max_capacity The maximum number of words the vocabulary can hold.
 * @return vocabulary_t The newly created vocabulary.
 */
vocabulary_t vocab_create(unsigned long options, size_t max_capacity) {
	vocabulary_internal_t* vocab = (vocabulary_internal_t*)malloc(sizeof(vocabulary_internal_t));
	if( ! vocab ) {
		DEBUG_MSG("Failed to allocate memory for vocabulary");
		return NULL; // allocation failed
	}

	vocab->options = options;
	vocab->words_list = NULL;
	vocab->words_tree = d_btree_create((d_btree_compare_func_t)strcmp, free);

	return vocab;
}

/**
 * @brief Destroys a vocabulary.
 * @param vocab The vocabulary to destroy.
 */
void vocab_destroy(vocabulary_t vocab) {
	ASSERT(vocab);
	destroy_words_list(vocab);
	d_btree_destroy(vocab->words_tree);
	free(vocab);
}

/**
 * @brief Adds a word to the vocabulary.
 * @param vocab The vocabulary to add the word to.
 * @param word The word to add.
 * @return bool True if the word was added, false if it already exists or if the input is invalid.
 * @note Assumes the given word is a valid null-terminated trimmed string. Empty words are ignored.
 */
bool vocab_add_word(vocabulary_t vocab, const char* word) {
	ASSERT(vocab);
	ASSERT(word && *word);
	char* word_copy = strdup(word);
	if( ! word_copy ) {
		DEBUG_MSG("Failed to allocate memory for word copy");
		return false; // allocation failed
	}
	if( ! d_btree_add(vocab->words_tree, word_copy) ) {
		free(word_copy);
		return false;
	}
	return true;
}

/**
 * @brief Gets the size of the vocabulary.
 * @param vocab The vocabulary to get the size of.
 * @return size_t The number of words in the vocabulary.
 */
size_t vocab_size(vocabulary_t vocab) {
	ASSERT(vocab);
	return d_btree_size(vocab->words_tree);
}

/**
 * @brief Resets the vocabulary by clearing all words.
 * @param vocab The vocabulary to reset.
 */
void vocab_reset(vocabulary_t vocab) {
	ASSERT(vocab);
	destroy_words_list(vocab);
	d_btree_reset(vocab->words_tree);
}

/**
 * @brief Gets all the words in the vocabulary.
 * @param vocab The vocabulary to get the words from.
 * @return d_array_t An array of words.
 */
d_array_t vocab_get_words(vocabulary_t vocab) {
	ASSERT(vocab);
	d_array_t a = d_array_create(0, free);
	iterator_t it = d_btree_iter(vocab->words_tree);
	_foreach(it, result) {
		d_array_add(a, strdup((const char*)result.value));
	}
	return a;
}

#ifdef _DEBUG_
/**
 * @brief Prints the vocabulary for debugging purposes.
 * @param vocab The vocabulary to print.
 */
void vocab_print(vocabulary_t vocab) {
	ASSERT(vocab);
	printf("Auto-Complete Vocabulary:\n");
	iterator_t it = d_btree_iter(vocab->words_tree);
	_foreach(it, result) {
		printf("%s\n", (const char*)result.value);
	}
}
#endif
