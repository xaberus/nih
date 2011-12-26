#ifndef _MEMORY_H
#define _MEMORY_H

#include "err.h"
#include <stdint.h>
#include <malloc.h>

typedef void * (mem_realloc_t)(void * ud, void * ptr, size_t osize, size_t nsize);

typedef struct {
  mem_realloc_t * realloc;
  void          * ud;
} mema_t;

void * plain_realloc(void * ud, void * ptr, size_t osize, size_t nsize);

void * mem_alloc(const mema_t * a, size_t size);
void * mem_realloc(const mema_t * a, void * data, size_t osize, size_t nsize);
void   mem_free(const mema_t * a, void * data);

#endif /* _MEMORY_H */
