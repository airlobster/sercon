#ifndef _CGLOB_H
#define _CGLOB_H

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

typedef void* cglob_iterator_t;

cglob_iterator_t globIterator(const char* pattern, unsigned long options);
const char* nextGlob(cglob_iterator_t iterator);
void freeGlobIterator(cglob_iterator_t iterator);

#ifdef __cplusplus
}
#endif

#endif /* _CGLOB_H */
