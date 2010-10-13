#ifndef _SX_POOL_H
#define _SX_POOL_H

#include <stdlib.h>
#include <stdint.h>

/*
 * [[#next#][#size#]{#size#}{data ...}{#size#}{data ...}....
 * |     header    |0                |1                |
 * |               |header  |data    |header  |data    |
 */

struct chunk_header {
  struct chunk_header * prev;
  size_t size;
};

#define SX_POOL_DATA_FLAG_FREE 0xff
#define SX_POOL_DATA_FLAG_CACHE 0xff00

/* waste memory here, for 16 byte aligment */
struct data_header {
  size_t size;
  uint64_t flags;
};

#define SX_POOL_FCACHE_UBITS_MIN 1
#define SX_POOL_FCACHE_UBITS_MAX 32

#define SX_POOL_FCACHE_MIN (SX_POOL_FCACHE_UBITS_MIN << 4)
#define SX_POOL_FCACHE_MAX (SX_POOL_FCACHE_UBITS_MAX << 4)

#define SX_POOL_FCACHE_SIZE 64
#define SX_POOL_CHUNK_ALLOC (4096 - sizeof(struct data_header) - sizeof(struct data_header))

struct cache_array {
  size_t size;
  struct data_header * data[SX_POOL_FCACHE_SIZE];
};


struct sx_pool {
  struct chunk_header * head;
  struct chunk_header * tail;

  unsigned int counter;

  struct cache_array fcache[SX_POOL_FCACHE_UBITS_MAX];
};
typedef struct sx_pool sx_pool_t;

void   sx_pool_init(sx_pool_t * pool);
void   sx_pool_clear(sx_pool_t * pool);

void * sx_pool_getmem(sx_pool_t * pool, size_t size);

void   sx_pool_normalize(sx_pool_t * pool);
void   sx_pool_retmem(sx_pool_t * pool, void * data);

#endif /* _SX_POOL_H */
