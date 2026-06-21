#ifndef _VOCABULARY_H_
#define _VOCABULARY_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _vocabulary_internal_t* vocabulary_t;

typedef enum {
	VOCAB_OPT_CASE_INSENSITIVE = 1UL << 0,
} vocabulary_option_t;

vocabulary_t vocab_create(unsigned long options, size_t max_capacity);
void vocab_destroy(vocabulary_t vocab);
bool vocab_add_word(vocabulary_t vocab, const char* word);
size_t vocab_size(vocabulary_t vocab);
char** vocab_get_words(vocabulary_t vocab);

#ifdef _DEBUG_
void vocab_print(vocabulary_t vocab);
#endif

#ifdef __cplusplus
}
#endif

#endif // _VOCABULARY_H_
