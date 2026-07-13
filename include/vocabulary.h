#ifndef _VOCABULARY_H_
#define _VOCABULARY_H_

/**
 * @file vocabulary.h
 * @brief Vocabulary management functions
 *
 */

#include <stddef.h>
#include <stdbool.h>
#include "d_array.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque type for vocabulary
 */
typedef struct _vocabulary_internal_t* vocabulary_t;

vocabulary_t vocab_create(unsigned long options, size_t max_capacity);
void vocab_destroy(vocabulary_t vocab);
bool vocab_add_word(vocabulary_t vocab, const char* word);
size_t vocab_size(vocabulary_t vocab);
d_array_t vocab_get_words(vocabulary_t vocab);
void vocab_reset(vocabulary_t vocab);

#ifdef _DEBUG_
void vocab_print(vocabulary_t vocab);
#endif

#ifdef __cplusplus
}
#endif

#endif // _VOCABULARY_H_
