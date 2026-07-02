#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "vocabulary.h"
#include "utils.h"
#include "r_btree.h"

/**
 * @brief The internal structure of a vocabulary.
 */
typedef struct _vocabulary_internal_t {
	unsigned long options; /**< Options for the vocabulary. */
	r_btree_t words_tree; /**< The binary tree for fast word lookup. */
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
	vocab->words_tree = r_btree_create((r_btree_compare_func_t)strcmp, free);

	return vocab;
}

/**
 * @brief Destroys a vocabulary.
 * @param vocab The vocabulary to destroy.
 */
void vocab_destroy(vocabulary_t vocab) {
	ASSERT(vocab);
	destroy_words_list(vocab);
	r_btree_destroy(vocab->words_tree);
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
	if( ! r_btree_add(vocab->words_tree, word_copy) ) {
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
	return r_btree_size(vocab->words_tree);
}

/**
 * @brief Callback function for enumerating words.
 * @param data The data passed to the callback (the word).
 * @param context Context data passed to the callback.
 */
static void enum_words_callback(void* data, void* context) {
	ASSERT(context);
	char*** words_ptr = (char***)context;
	ASSERT(*words_ptr);
	**words_ptr = strdup((const char*)data); // duplicate the word
	(*words_ptr)++;
}

/**
 * @brief Gets all the words in the vocabulary.
 * @param vocab The vocabulary to get the words from.
 * @return char** A null-terminated array of words.
 * @note The returned array is owned by the vocabulary and should not be freed by the caller.
 */
char** vocab_get_words(vocabulary_t vocab) {
	ASSERT(vocab);
	size_t size = r_btree_size(vocab->words_tree);
	char** words = (char**)malloc(sizeof(char*) * (size+1));
	if( ! words ) {
		DEBUG_MSG("Failed to allocate memory for words list");
		return NULL; // allocation failed
	}
	char** words_ptr = words;
	// fetch words from the binary tree
	r_btree_traverse(vocab->words_tree, (r_btree_traverse_func_t)enum_words_callback, &words_ptr);
	// null-terminate the array
	words[size] = 0;
	// replace the cached words list
	destroy_words_list(vocab);
	vocab->words_list = words;
	return vocab->words_list;
}

#ifdef _DEBUG_
/**
 * @brief Prints the vocabulary for debugging purposes.
 * @param vocab The vocabulary to print.
 */
void vocab_print(vocabulary_t vocab) {
	ASSERT(vocab);
	printf("Auto-Complete Vocabulary (size: %zu):\n", r_btree_size(vocab->words_tree));
	char** words = vocab_get_words(vocab);
	if( ! words ) return;
	for(char** w = words; w && *w; w++) {
		printf("%*ld: %s\n", numdigits(r_btree_size(vocab->words_tree), 0), w - words + 1, *w);
	}
}
#endif
