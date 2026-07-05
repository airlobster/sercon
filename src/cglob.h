#ifndef _CGLOB_H
#define _CGLOB_H

/**
 * @file cglob.h
 * @brief Cross-platform globbing library for C.
 *
 * This library provides a simple interface for performing file globbing
 * operations in a cross-platform manner. It supports various file types
 * and allows users to iterate over matching files based on specified patterns.
 *
 * @note This library is designed to be used in both C and C++ projects.
 */

#include "iterator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	CGLOB_FILE_REGULAR = 1UL << 0, /**< Regular file */
	CGLOB_FILE_DIRECTORY = 1UL << 1, /**< Directory */
	CGLOB_FILE_SYMLINK = 1UL << 2, /**< Symbolic link */
	CGLOB_FILE_CHAR_DEVICE = 1UL << 3, /**< Character device */
	CGLOB_FILE_BLOCK_DEVICE = 1UL << 4, /**< Block device */
	CGLOB_FILE_FIFO = 1UL << 5, /**< FIFO (named pipe) */
	CGLOB_FILE_SOCKET = 1UL << 6, /**< Socket */
} cglob_file_type_t;

iterator_t cglob_iterator(const char* patterns[], unsigned long options);

#ifdef __cplusplus
}
#endif

#endif /* _CGLOB_H */
