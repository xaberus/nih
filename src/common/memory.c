#include "memory.h"

void * plain_realloc(void * ud, void * ptr, size_t osize, size_t nsize)
{
  (void) ud;  (void) osize;  /* not used */
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return realloc(ptr, nsize);
}

void * mem_alloc(const mem_allocator_t * a, size_t size)
{
  return a->realloc(a->ud, NULL, 0, size);
}

void * mem_realloc(const mem_allocator_t * a, void * data, size_t osize, size_t nsize)
{
  return a->realloc(a->ud, data, osize, nsize);
}

void mem_free(const mem_allocator_t * a, void * data)
{
  (void) a->realloc(a->ud, data, 1 /* not 0 ? */, 0);
}
