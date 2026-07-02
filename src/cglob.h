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

typedef void(*cglob_callback_t)(const char* path, void* context);

int cglob(const char* pattern, unsigned long options, cglob_callback_t callback, void* context);

#ifdef __cplusplus
}
#endif

#endif /* _CGLOB_H */
