#ifndef _BLOBLTREE_H
#define _BLOBLTREE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef TEST
# include <bt.h>
#endif

/* never forget: everything is a stream of bytes */

/* one blob per node */
typedef struct blob blob_t;
typedef struct pool pool_t;
typedef struct poolmem poolmem_t;


#define BLOB_DESTRUCTOR_ARGS blob_t *
typedef int (blob_destructor_t)(BLOB_DESTRUCTOR_ARGS);


typedef struct blob_vtable blob_vtable_t;
struct blob_vtable {
  blob_destructor_t * destructor;
};

struct blob_flags {
  unsigned int free : 1;
  unsigned int pool : 1;
  unsigned int poolmem : 1;
  unsigned int _reserved0 : 5;

  unsigned int first : 1;
  unsigned int destructed : 1;
  unsigned int indestructible : 1;
  unsigned int _reserved1 : 5;

  unsigned int check : 16;
  unsigned int uid : 32;
};


#define BLOBTREE_RECORD \
  blob_t              * rev; \
  blob_t              * next; \
  blob_t              * child; \
  const blob_vtable_t * vtable; \
  size_t                size; \
  struct blob_flags     flags; \
  const char          * name; \


/* blob hierarchy */
struct blob {
  BLOBTREE_RECORD
  pool_t * pool;
};

struct pool {
  BLOBTREE_RECORD
  void * data;
};

#define ALIGN16(_size) (((_size) + 15L) & ~15L)
#define ALIGN4096(_size) (((_size) + 4095L) & ~4095L)

pool_t * pool_alloc(blob_t * ctx, size_t size);

blob_t * blob_alloc(blob_t * ctx, size_t size);
int      blob_free(blob_t * ctx, blob_t * null_ctx);
blob_t * blob_realloc(blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx);

size_t   blob_total_blobs(blob_t * blob);
size_t   blob_total_size(blob_t * blob);

#endif /* _BLOBLTREE_H */

// vim: filetype=c:expandtab:shiftwidth=2:tabstop=4:softtabstop=2:encoding=utf-8:textwidth=100
