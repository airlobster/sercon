#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <sqlite3.h>
#include "vocabulary.h"
#include "utils.h"

/**
 * @brief The internal structure of a vocabulary.
 */
typedef struct _vocabulary_internal_t {
	unsigned long options;
	sqlite3* db;
	size_t size;
	char** words_list;
	bool dirty;
	size_t max_capacity;
} vocabulary_internal_t;

/**
 * @brief Initializes the database for the vocabulary.
 * @param vocab The vocabulary whose database is to be initialized.
 * @return int SQLITE_OK on success, or an SQLite error code on failure.
 */
static int init_db(vocabulary_internal_t* vocab) {
	const char* sql =
		"CREATE TABLE IF NOT EXISTS words ("
		"word TEXT PRIMARY KEY,"
		"timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
		");";
	int rc = sqlite3_exec(vocab->db, sql, 0, 0, 0);
	if( rc != SQLITE_OK ) {
#ifdef _DEBUG_
		fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(vocab->db));
#endif
	}
	return rc;
}

/**
 * @brief Removes the oldest words from the vocabulary if it exceeds the maximum capacity.
 * @param vocab The vocabulary to remove words from.
 * @return int SQLITE_OK on success, or an SQLite error code on failure.
 */
static int remove_oldest_words(vocabulary_internal_t* vocab) {
	ASSERT(vocab);
	ASSERT(vocab->db);
	if( ! vocab->max_capacity || vocab->size <= vocab->max_capacity ) {
		return SQLITE_OK;
	}
	const char* sql =
		"DELETE FROM words WHERE word NOT IN "
		"(SELECT word FROM words ORDER BY timestamp DESC LIMIT ?);"
		;
	sqlite3_stmt *stmt;
	int rc = sqlite3_prepare_v2(vocab->db, sql, -1, &stmt, 0);
	if( rc != SQLITE_OK ) {
#ifdef _DEBUG_
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(vocab->db));
#endif
		return rc;
	}
	sqlite3_bind_int(stmt, 1, (int)(vocab->max_capacity));
	rc = sqlite3_step(stmt);
	if( rc != SQLITE_DONE ) {
#ifdef _DEBUG_
		fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(vocab->db));
#endif
		sqlite3_finalize(stmt);
		return rc;
	}
	sqlite3_finalize(stmt);
	vocab->size = vocab->max_capacity;
	return SQLITE_OK;
}

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
	vocab->words_list = 0;
}

/**
 * @brief Creates a new vocabulary.
 * @param options Options for the vocabulary (e.g., case-insensitive).
 * @param max_capacity The maximum number of words the vocabulary can hold.
 * @return vocabulary_t The newly created vocabulary.
 */
vocabulary_t vocab_create(unsigned long options, size_t max_capacity) {
	vocabulary_internal_t* vocab = (vocabulary_internal_t*)malloc(sizeof(vocabulary_internal_t));
	ASSERT(vocab);
	if( ! vocab ) return 0; // allocation failed

	vocab->options = options;
	vocab->size = 0;
	vocab->db = 0;
	vocab->dirty = true;
	vocab->words_list = 0;
	vocab->max_capacity = max_capacity;

	if( sqlite3_open(":memory:", &vocab->db) != SQLITE_OK ) {
#ifdef _DEBUG_
		fprintf(stderr, "Failed to open in-memory database: %s\n", sqlite3_errmsg(vocab->db));
#endif
		free(vocab);
		return 0; // failed to open in-memory database
	}

	if( init_db(vocab) != SQLITE_OK ) {
		vocab_destroy(vocab);
		vocab = 0;
		return 0; // failed to initialize database
	}

	return vocab;
}

/**
 * @brief Destroys a vocabulary.
 * @param vocab The vocabulary to destroy.
 */
void vocab_destroy(vocabulary_t vocab) {
	ASSERT(vocab);
	destroy_words_list(vocab);
	if( vocab->db ) {
		sqlite3_close(vocab->db);
	}
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
	ASSERT(vocab->db);
	sqlite3_stmt *stmt;
	sqlite3_prepare_v2(vocab->db, "INSERT INTO words (word) VALUES (?);", -1, &stmt, 0);
	sqlite3_bind_text(stmt, 1, word, -1, SQLITE_STATIC);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if( rc == SQLITE_DONE ) {
		vocab->dirty = true;
		vocab->size++;
		return remove_oldest_words(vocab) == SQLITE_OK;
	} else if( rc != SQLITE_CONSTRAINT ) {
#ifdef _DEBUG_
		fprintf(stderr, "Failed to add word '%s': %s\n", word, sqlite3_errmsg(vocab->db));
#endif
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
	ASSERT(vocab->db);
	return vocab->size;
}

/**
 * @brief Callback function for enumerating words in the database.
 * @param user_data User data passed to the callback.
 * @param argc The number of columns in the result.
 * @param argv The values of the columns.
 * @param azColName The names of the columns.
 * @return int 0 to continue, non-zero to abort.
 */
static int enum_words_callback(void* user_data, int argc, char** argv, char** azColName) {
	(void)azColName; // unused parameter
	if( argc > 0 && argv[0] ) {
		char*** words_ptr = (char***)user_data;
		ASSERT(words_ptr && *words_ptr);
		**words_ptr = strdup(argv[0]); // duplicate the word
		(*words_ptr)++;
	}
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
	ASSERT(vocab->db);
	if( vocab->words_list && ! vocab->dirty ) {
		return vocab->words_list; // return cached list
	}
	// allocate memory for the words list
	char** words = (char**)malloc(sizeof(char*) * (vocab->size+1));
	if( ! words ) return 0; // allocation failed
	char** words_ptr = words;
	// fetch words from the database
	int rc = sqlite3_exec(vocab->db,
		"SELECT word FROM words ORDER BY word ASC;", enum_words_callback, &words_ptr, 0);
	if( rc != SQLITE_OK ) {
#ifdef _DEBUG_
		fprintf(stderr, "Failed to enumerate words: %s\n", sqlite3_errmsg(vocab->db));
#endif
		free(words);
		return 0; // error occurred
	}
	// null-terminate the array
	words[vocab->size] = 0;
	// replace the cached words list
	destroy_words_list(vocab);
	vocab->words_list = words;
	vocab->dirty = false;
	return vocab->words_list;
}

#ifdef _DEBUG_
/**
 * @brief Prints the vocabulary for debugging purposes.
 * @param vocab The vocabulary to print.
 */
void vocab_print(vocabulary_t vocab) {
	ASSERT(vocab);
	printf("Auto-Complete Vocabulary (size: %zu):\n", vocab->size);
	char** words = vocab_get_words(vocab);
	for(char** w = words; w && *w; w++) {
		printf("%*ld: %s\n", numdigits(vocab->size), w - words + 1, *w);
	}
}
#endif
