#ifndef _STRTOK_ITER_H_
#define _STRTOK_ITER_H_

/**
 * @file strtok_iter.h
 * @brief Header file for the strtok_iter function.
 *
 * This header file declares the strtok_iter function, which creates an iterator
 * that tokenizes a string based on specified delimiters.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "iterator.h"

iterator_t strtok_iter(const char *str, const char *delim);

#ifdef __cplusplus
}
#endif

#endif //_STRTOK_ITER_H_
