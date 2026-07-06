#ifndef _STRTOK_ITER_H_
#define _STRTOK_ITER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "iterator.h"

iterator_t strtok_iter(const char *str, const char *delim);

#ifdef __cplusplus
}
#endif

#endif //_STRTOK_ITER_H_
