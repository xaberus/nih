#ifndef _GC_PRIVATE_H
#define _GC_PRIVATE_H

#include "gc.h"

#if 0
void   gc_mem_free(gc_global_t * g, size_t size, void * p);
void * gc_mem_realloc(gc_global_t * g, size_t osz, size_t nsz, void * p);

#define gc_mem_new(g, s) \
  gc_mem_realloc(g, 0, (s), NULL)
#endif

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

/* vector */

#define gc_vector_init(_err, _g, _v, _init) \
  do { \
    __typeof__(_v) __v = (_v); \
    __v->count = 0; \
    __v->dsize = (_init); \
    __v->data = malloc(__v->dsize * sizeof(__v->data[0])); \
    if (!__v->data) { \
      (_err) = err_return(ERR_MEM_USE_ALLOC, "malloc() failed"); \
    } else { \
      __v->psize = 0; \
      __v->proc = NULL; \
      memset(__v->data, 0, sizeof(__typeof__(__v->data[0])) * __v->dsize); \
    } \
  } while (0)

#define gc_vector_init_basic(_g, _v, _init) \
  do { \
    __typeof__(_v) __v = (_v); \
    __v->count = 0; \
    __v->dsize = (_init); \
    __v->data = malloc(__g, __v->dsize * sizeof(__v->data[0])); \
    if (!__v->data) { \
      (_err) = err_return(ERR_MEM_USE_ALLOC, "malloc() failed"); \
    } else { \
      memset(__v->data, 0, sizeof(__typeof__(__v->data[0])) * __v->dsize); \
    } \
  } while (0)


#define gc_vector_clear(_g, _v) \
  do { \
    __typeof__(_v) __v = (_v); \
    if (__v->data) { \
      free(__v->data); \
      __v->data = NULL; \
      __v->dsize = 0; \
      __v->count = 0; \
    } \
    if (__v->proc) { \
      free(__v->proc); \
      __v->proc = NULL; \
      __v->psize = 0; \
    } \
  } while (0)

#define gc_vector_clear_basic(_g, _v) \
  do { \
    __typeof__(_v) __v = (_v); \
    if (__v->data) { \
      free(__v->data); \
      __v->data = NULL; \
      __v->dsize = 0; \
      __v->count = 0; \
    } \
  } while (0)


#define gc_vector_reset(_g, _v) \
  do { \
    __typeof__(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(__typeof__(__v->data[0])) * __v->dsize); \
    memset(__v->proc, 0, sizeof(__typeof__(__v->proc[0])) * __v->psize); \
  } while (0)

#define gc_vector_reset_basic(_g, _v) \
  do { \
    __typeof__(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(__typeof__(__v->data[0])) * __v->dsize); \
  } while (0)

#define gc_vector_process(_err, _g, _v, _grow) \
  do { \
    __typeof__(_v) __v = (_v); \
    size_t __grow = (_grow); \
    if (__v->proc) { \
      void * __tmp = realloc(__v->proc, __grow * sizeof(__v->proc[0])); \
      if (!__tmp) { \
        (_err) = err_return(ERR_MEM_REALLOC, "realloc() failed"); \
      } else { \
      __v->proc = __tmp; \
      __v->psize = __grow; \
      } \
    } else { \
      __v->proc = malloc(__grow * sizeof(__v->proc[0])); \
      if (!__v->proc ) { \
        (_err) = err_return(ERR_MEM_REALLOC, "malloc() failed"); \
      } else { \
        __v->psize = __grow; \
      } \
    } \
    memset(__v->proc, 0, __v->psize * sizeof(__v->proc[0])); \
  } while (0)

#define gc_vector_resize(_err, _g, _v, _grow) \
  do { \
    __typeof__(_v) __v = (_v); \
    size_t __count = (_grow); \
    size_t __grow = ALIGN16(__count); \
    if (__grow > __v->dsize) { \
      void * __tmp = realloc(__v->data, __grow * sizeof(__v->data[0])); \
      if (!__tmp) { \
        (_err) = err_return(ERR_MEM_USE_ALLOC, "realloc() failed"); \
      } else { \
        __v->data = __tmp; \
        memset(&__v->data[__v->dsize], 0, (__grow - __v->dsize) * sizeof(__v->data[0])); \
      } \
    } else { \
      memset(&__v->data[__grow], 0, (__v->dsize - __grow) * sizeof(__v->data[0])); \
      __v->count = __count; \
    } \
    __v->dsize = __grow; \
  } while (0)

#define gc_vector_swap(_g, _v) \
  do { \
    __typeof__(_v) __v = (_v); \
    if (__v->proc) { \
      __typeof__(__v->proc) __tmp = __v->data; \
      size_t __tsize = __v->dsize; \
      __v->data = __v->proc; \
      __v->dsize = __v->psize; \
      __v->count = 0; \
      __v->psize = __tsize; \
      __v->proc = __tmp; \
    } \
  } while (0)

#define gc_vector_append(_err, _g, _v, _value) \
  do { \
    __typeof__(_v) __v = (_v); \
    if (!(__v->count < __v->dsize)) { \
      size_t __sz = ALIGN16(__v->count + 1); \
      void * __tmp = realloc(__v->data, __sz * sizeof(__v->data[0])); \
      if (!__tmp) { \
        (_err) = err_return(ERR_MEM_USE_ALLOC, "realloc() failed"); \
      } else { \
        __v->data = __tmp; \
        __v->dsize = __sz; \
      } \
    } \
    __v->data[__v->count++] =  _value; \
  } while (0)

#define gc_vector_remove_all(_g, _v, _value) \
  do { \
    __typeof__(_v) __v = (_v); \
    __typeof__(_value) value = (_value); \
    for (size_t k = 0; k < __v->count; k++) { \
      if (__v->data[k] == value) { \
        for (size_t l = k + 1, m = __v->count; l < m; l++) { \
          __v->data[l - 1] = __v->data[l]; \
        } \
        __v->count--; \
      } \
    } \
  } while (0)

/* list */
inline static
void gch_prepend(gc_hdr_t ** head, gc_hdr_t * node)
{
  node->next = *head;
  *head = node;
}

inline static
void gch_unlink(gc_hdr_t ** head, gc_hdr_t ** prev, gc_hdr_t * node)
{
  *prev = node->next;
  if (node == *head) {
    *head = node->next;
  }
}

#define gc_head_prepare(_l) \
  do { \
    __typeof__(_l) __l = (_l); \
    __l->sweep = &__l->head; \
  } while (0)

#define gc_head_reset(_l) \
  do { \
    __typeof__(_l) __l = (_l); \
    __l->head = 0; \
    __l->sweep = &__l->head; \
  } while (0)

inline static
void gcl_prepend(gc_obj_t ** head, gc_obj_t * node)
{
  node->list = *head;
  *head = node;
}

#define gc_list_reset(_l) \
  do { \
    __typeof__(_l) __l = (_l); \
    __l->head = 0; \
  } while (0)

inline static
gc_obj_t * gcl_pop(gc_obj_t ** head)
{
  gc_obj_t * r = *head;
  if (*head) {
    *head = (*head)->list;
  }
  return r;
}

inline static
void gclp_insert(gc_obj_t ** loop, gc_obj_t * node)
{
  if (*loop) {
    gc_obj_t * c = *loop;
    node->gch.next = c->gch.next;
    c->gch.next = &node->gch;
    *loop = node;
  } else {
    node->gch.next = &node->gch;
    *loop = node;
  }
}

#define gc_loop_reset(_l) \
  do { \
    __typeof__(_l) __l = (_l); \
    __l->loop = 0; \
  } while (0)

inline static
gc_obj_t * gclp_pop(gc_obj_t ** loop)
{
  gc_obj_t * r = NULL;
  if (*loop) {
    r = (gc_obj_t *) GC_HDR(*loop)->next;
    if (*loop == r) {
      *loop = NULL;
    } else {
      GC_HDR(*loop)->next = GC_HDR(r)->next;
    }
  }
  return r;
}

typedef __typeof__(((gc_global_t *) NULL)->strings) gc_strings_t;
typedef __typeof__(((gc_global_t *) NULL)->strings.buckets) gc_buckets_t;
typedef __typeof__(*((gc_global_t *) NULL)->strings.buckets.data) gc_bucket_t;
typedef __typeof__(((gc_global_t *) NULL)->objects) gc_ostore_t;
typedef __typeof__(((gc_global_t *) NULL)->headers) gc_hstore_t;

inline static
uint32_t other_white(gc_global_t * g)
{
  return g->white ^ GC_FLAG_WHITES;
}

#define is_dead(o, ow)   ((GC_HDR(o)->flag & ow) & GC_FLAG_WHITES)
#define flip_white(x) (GC_HDR(x)->flag ^= GC_FLAG_WHITES)

#if 0
inline static
void __attribute__((format(printf, 2, 3))) log(unsigned N, const char * fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  switch (N) {
    case 0:
      // vfprintf(stderr, fmt, ap);
      break;
    case 1:
      // vfprintf(stderr, fmt, ap);
      break;
    case 2:
      // vfprintf(stderr, fmt, ap);
      break;
    case 3:
      // vfprintf(stderr, fmt, ap);
      break;
    case 4:
      // vfprintf(stderr, fmt, ap);
      break;
    case 5:
      // vfprintf(stderr, fmt, ap);
      break;
    case 6:
      // vfprintf(stderr, fmt, ap);
      break;
    default:
      break;
  }
  va_end(ap);
}
#else
  #define log(...)
#endif

#define log_obj(_N, _x, _y) \
  log(_N, "# %s(g = %p [%s%s], obj = %p [%s%s%s][%u])\n", \
    __FUNCTION__, \
    (void *) (_x), \
    (_x)->white & GC_FLAG_WHITE0 ? "w0" : "", \
    (_x)->white & GC_FLAG_WHITE1 ? "w1" : "", \
    (void *) _y, \
    GC_HDR(_y)->flag & GC_FLAG_WHITE0 ? "w0" : "", \
    GC_HDR(_y)->flag & GC_FLAG_WHITE1 ? "w1" : "", \
    GC_HDR(_y)->flag & GC_FLAG_BLACK ? "b" : "", \
    obj_size(_y))

inline static
void intern_obj(gc_global_t * g, gc_obj_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES);
  GC_HDR(o)->size = size;
  gch_prepend(GC_HDRP(&g->objects.head), GC_HDR(o));
  g->total++;
}

inline static
void intern_hdr(gc_global_t * g, gc_hdr_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES);
  GC_HDR(o)->size = size;
  gch_prepend(GC_HDRP(&g->headers.head), GC_HDR(o));
  g->total++;
}

inline static
err_r * strings_resize(gc_strings_t * ss, uint32_t newmask)
{
  log(0, "# %s(%08x->%08x)\n", __FUNCTION__, ss->mask, newmask);
  gc_buckets_t * buckets = &ss->buckets;

  {
    err_r * err = NULL;
    gc_vector_process(err, g, buckets, newmask + 1);
    if (err) {
      return err;
    }
  }

  for (size_t i = 0; i < buckets->dsize; i++) {
    gc_bucket_t * b = &buckets->data[i];
    gc_str_t    * p = b->head, * n;
    while (p) {
      n = (gc_str_t *) p->gch.next;
      assert((p->hash & newmask) < buckets->psize);
      gch_prepend(GC_HDRP(&buckets->proc[p->hash & newmask].head), GC_HDR(p));
      p = n;
    }
  }
  gc_vector_swap(g, buckets);

  for (uint32_t k = 0; k < buckets->dsize; k++) {
    gc_bucket_t * n = &buckets->data[k];
    n->sweep = &n->head;
  }
  ss->mask = newmask;

  return NULL;
}

inline static
err_r * intern_str(gc_global_t * g, gc_str_t * s, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) s, size);
  GC_HDR(s)->flag = (g->white & GC_FLAG_WHITES);
  GC_HDR(s)->size = size;
  gch_prepend(GC_HDRP(&g->strings.buckets.data[s->hash & g->strings.mask].head), GC_HDR(s));
  if (g->strings.count++ > g->strings.mask) {
    uint32_t newmask = (g->strings.mask << 1) | 1;
    if (g->state != GC_STATE_SWEEP_STRING && newmask < 0xffffff - 1) {
      err_r * err =  strings_resize(&g->strings, newmask);
      if (err) {
        return err_return(ERR_FAILURE, "strings_resize() failed");
      }
    }
  }
  return NULL;
}

extern gc_vtable_t gc_str_vtable;

#endif /* _GC_PRIVATE_H */
