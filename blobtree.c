
#include "blobtree.h"
#include <string.h>

#include <sexp.h>

#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX_BLOB_SIZE 0x10000000
#define BLOB_CHECK 0xd731 /* == 0y1101011100110001 */

//#define BLOB_TO_DATA(_blob) ((void *)(BLOB_HDR_SIZE + (char*)(_blob)))


static inline
blob_t * blob_check_context(blob_t * ctx) {
  if (!ctx)
    return NULL;
  if (ctx->flags.check != BLOB_CHECK)
    return NULL;
  if (ctx->flags.free)
    return NULL;

  return ctx;
}

size_t blob_total_blobs(blob_t * blob) {
  size_t total = 0;
  blob_t * c;

  blob = blob_check_context(blob);

  if (!blob)
    return 0;

  total++;
  for (c = blob->child; c; c = c->next)
    total += blob_total_blobs(c);

  return total;
}

size_t blob_total_size(blob_t * blob) {
  size_t total = 0;
  blob_t * c;

  blob = blob_check_context(blob);

  if (!blob)
    return 0;

  total += blob->size;

  for (c = blob->child; c; c = c->next)
    total += blob_total_size(c);

  return total;
}

pool_t * pool_alloc(blob_t * ctx, size_t size)
{
  size = ALIGN4096(size + sizeof(pool_t));

  pool_t * pool = (pool_t *)blob_alloc(ctx, size);
  if (!pool)
    return NULL;

  pool->flags.pool = 1;
  pool->data = (char *)pool + sizeof(pool_t);

  return  pool;
}

static inline
blob_t * blob_plain_alloc(size_t size)
{
  blob_t * blob = size ? malloc(size) : NULL;
  if (!blob)
    return NULL;

  blob->flags = ((struct blob_flags){ .free=0, });
  blob->flags.first = 1;
  blob->flags.check = BLOB_CHECK;

  blob->size = size;
  blob->vtable = NULL;
  blob->child = NULL;
  blob->rev = NULL;
  blob->next = NULL;
  blob->rev = NULL;

  return blob;
}

static inline
blob_t * pool_blob_alloc(pool_t * pool, size_t size)
{
  size_t space_left;
  blob_t * blob;
  size_t blob_size;

  pool = (pool_t *)blob_check_context((blob_t *)pool);
  if (!pool)
    return NULL;

  space_left = ((char *)pool + pool->size - sizeof(pool_t)) - ((char *)pool->data);
  blob_size = ALIGN16(size);

  if (space_left < blob_size)
    return NULL;

  blob = pool->data;
  blob->size = size;

  pool->data = ((char *)blob + blob_size);

  blob->flags = ((struct blob_flags){ .free=0, });
  blob->flags.first = 1;
  blob->flags.check = BLOB_CHECK;
  blob->flags.poolmem = 1;
  blob->pool = pool;

  blob->vtable = NULL;
  blob->child = NULL;
  blob->rev = NULL;
  blob->next = NULL;
  blob->rev = NULL;

  return blob;
}


static inline
blob_t * pool_blob_realloc(pool_t * pool, blob_t * ctx, blob_t * blob, size_t size)
{
  if (!blob || (blob_t *)pool != ctx || !size)
    return NULL;

  return pool_blob_alloc(pool, size);
}

static inline
blob_t * blob_pool_alloc(blob_t * ctx, size_t size)
{
  if (ctx->flags.pool)
    return pool_blob_alloc((pool_t *)ctx, size);
  else if (ctx->flags.poolmem)
    return pool_blob_alloc(ctx->pool, size);

  return NULL;
}

static inline
void _blob_list_prepend(blob_t * parent, blob_t * blob)
{
  blob_t ** list = &parent->child;
  if (!*list) {
    *list = blob;
    blob->flags.first = 1;
    blob->rev = parent;
    blob->next = NULL;
  } else {
    (*list)->flags.first = 0;
    (*list)->rev = blob;
    blob->next = *list;
    blob->flags.first = 1;
    blob->rev = parent;
    *list = blob;
  }
}

static inline
void _blob_list_unlink(blob_t * parent, blob_t * blob)
{
  blob_t ** list = &parent->child;
  if (blob == *list && blob->flags.first) {
    *list = blob->next;
    if (*list) {
      (*list)->flags.first = 1;
      (*list)->rev = parent;
    }
  } else {
      if (blob->rev)
        blob->rev->next = blob->next;
      if (blob->next)
        blob->next->rev = blob->rev;
  }
  /* cleanup */
  if (blob && (blob != *list)) {
    blob->flags.first = 1;
    blob->rev = NULL;
    blob->next = NULL;
  }
}

static inline
blob_t * blob_context_alloc(blob_t * ctx, size_t size)
{
  blob_t * blob = blob_pool_alloc(ctx, size);

  if (!blob) {
    blob = blob_plain_alloc(size);
  }

  if (blob)
    _blob_list_prepend(ctx, blob);

  return blob;
}

blob_t * blob_alloc(blob_t * ctx, size_t size)
{
  if (size < sizeof(blob_t) || size >= MAX_BLOB_SIZE)
    return NULL;

  ctx = blob_check_context(ctx);

  if (ctx)
    return blob_context_alloc(ctx, size);
  else
    return blob_plain_alloc(size);
}

static inline
void _blob_consume(blob_t * blob)
{
  if (blob->flags.first && blob->rev) {
    _blob_list_unlink(blob->rev, blob);
  } else {
    if (blob->rev)
      blob->rev->next = blob->next;
    if (blob->next)
      blob->next->rev = blob->rev;
  }
}

static inline
blob_t * _blob_parent(blob_t * blob) {
  while (blob && !blob->flags.first)
    blob = blob->rev;
  return blob->rev;
}

#if 0
static inline
int _blob_is_parent(blob_t * ctx, blob_t * parent) {
  if (!ctx)
    return 0;

  blob_t * p = blob_check_context(ctx);
  while (p) {
    if (p == parent)
      return 1;
    p = _blob_parent(p);
  }

  return 0;
}
#endif

static inline
int _blob_free_r(blob_t * root, blob_t * parent, blob_t * blob, blob_t * null_ctx)
{
  blob_t * child;
  int err = 0, ret = 0;
  UNUSED_PARAM(parent);

  if (blob->vtable && blob->vtable->destructor && !blob->flags.destructed) {
    if (blob->flags.indestructible)
      return 2;

    blob->flags.destructed = 1;
    if (blob->vtable->destructor(blob)) {
      blob->flags.destructed = 0;
      blob->flags.indestructible = 1;
      return 2;
    }
  }

  _blob_consume(blob);

  while (blob->child) {
    child = blob->child;

    if ((err = _blob_free_r(root, blob, child, null_ctx))) {
      if (err == 2) {
        _blob_consume(child);
        _blob_list_prepend(null_ctx, child);
      }
      ret = 1;
    }
  }

  if (blob->flags.pool || blob->flags.poolmem) {
    pool_t * pool;

    if (blob->flags.pool)
      pool = (pool_t *)blob;
    else if (blob->flags.poolmem)
      pool = blob->pool;

    blob->flags.free = 1;
  } else {
    blob->flags.free = 1;
    free(blob);
  }

  return ret;
}

int blob_free(blob_t * blob, blob_t * null_ctx)
{
  blob = blob_check_context(blob);
  null_ctx = blob_check_context(null_ctx);

  if (!blob || (!null_ctx && blob_total_blobs(blob)!=1))
    return 1;

  return _blob_free_r(blob, NULL, blob, null_ctx);
}

static inline
blob_t * _blob_realloc(blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx)
{
  blob_t * new;
  bool malloced = false;

  blob->flags.free = 1;

  if (blob->flags.poolmem) {
    new = pool_blob_realloc(blob->pool, ctx, blob, size);

    if (!new) {
      new = blob_plain_alloc(size);
      malloced = true;
    }

    if (new)
      memcpy(new, blob, MIN(blob->size, size));
  } else {
    new = realloc(blob, size);
  }

  if (!new) {
    blob->flags.free = 0;
    return NULL;
  }

  new->flags.free = 0;
  if (malloced)
    new->flags.poolmem = 0;
  if (new->flags.first && new->rev)
    new->rev->child = new;
  if (new->child)
    new->child->rev = new;

  if (!new->flags.first && new->rev)
    new->rev->next = new;
  if (new->next)
    new->next->rev = new;

  new->size = size;

  return new;
}

blob_t * blob_realloc(blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx)
{
  if (size == 0) {
    blob_free(blob, null_ctx);
    return NULL;
  }

  if (size < sizeof(blob_t) || size >= MAX_BLOB_SIZE)
    return NULL;

  if (!blob)
    return blob_alloc(ctx, size);

  blob = blob_check_context(blob);

  if (!blob || blob->flags.pool)
    return NULL;

  if ((size < blob->size) && ((blob->size - size) < 1024)) {
    blob->size = size;
    return blob;
  }

  return _blob_realloc(ctx, blob, size, null_ctx);
}


#include "blobtree-test.c"

// vim: filetype=c:expandtab:shiftwidth=2:tabstop=4:softtabstop=2:encoding=utf-8:textwidth=100
