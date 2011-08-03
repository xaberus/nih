#ifndef _GC_PRIVATE_H
#define _GC_PRIVATE_H

#include "gc.h"

/* vector */
#define TYPEOF(a) __typeof__(a)

#define gc_vector_init(_g, _v, _init) \
  do { \
    TYPEOF(_v) __v = (_v); \
    gc_global_t * __g = (_g); \
    __v->count = 0; \
    __v->dsize = (_init); \
    __v->data = gc_mem_new(__g, __v->dsize * sizeof(__v->data[0])); \
    __v->psize = 0; \
    __v->proc = NULL; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
  } while (0)

#define gc_vector_init_basic(_g, _v, _init) \
  do { \
    TYPEOF(_v) __v = (_v); \
    gc_global_t * __g = (_g); \
    __v->count = 0; \
    __v->dsize = (_init); \
    __v->data = gc_mem_new(__g, __v->dsize * sizeof(__v->data[0])); \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
  } while (0)


#define gc_vector_clear(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    if (__v->data) { \
      gc_mem_free(__g, __v->dsize * sizeof(__v->data[0]), __v->data); \
      __v->data = NULL; \
      __v->dsize = 0; \
      __v->count = 0; \
    } \
    if (__v->proc) { \
      gc_mem_free(__g, __v->psize * sizeof(__v->proc[0]), __v->proc); \
      __v->proc = NULL; \
      __v->psize = 0; \
    } \
  } while (0)

#define gc_vector_clear_basic(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    if (__v->data) { \
      gc_mem_free(__g, __v->dsize * sizeof(__v->data[0]), __v->data); \
      __v->data = NULL; \
      __v->dsize = 0; \
      __v->count = 0; \
    } \
  } while (0)


#define gc_vector_reset(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
    memset(__v->proc, 0, sizeof(TYPEOF(__v->proc[0])) * __v->psize); \
  } while (0)

#define gc_vector_reset_basic(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
  } while (0)

#define gc_vector_process(_g, _v, _grow) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    size_t __grow = (_grow); \
    if (__v->proc) { \
      __v->proc = gc_mem_realloc(__g, __v->psize * sizeof(__v->proc[0]), __grow * sizeof(__v->proc[0]), __v->proc); \
      __v->psize = __grow; \
    } else { \
      __v->proc = gc_mem_new(__g, __grow * sizeof(__v->proc[0])); \
      __v->psize = __grow; \
    } \
    memset(__v->proc, 0, __v->psize * sizeof(__v->proc[0])); \
  } while (0)

#define gc_vector_resize(_g, _v, _grow) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    size_t __count = (_grow); \
    size_t __grow = ALIGN16(__count); \
    if (__grow > __v->dsize) { \
      __v->data = gc_mem_realloc(__g, __v->dsize * sizeof(__v->data[0]), __grow * sizeof(__v->data[0]), __v->data); \
      memset(&__v->data[__v->dsize], 0, (__grow - __v->dsize) * sizeof(__v->data[0])); \
    } else { \
      memset(&__v->data[__grow], 0, (__v->dsize - __grow) * sizeof(__v->data[0])); \
      __v->count = __count; \
    } \
    __v->dsize = __grow; \
  } while (0)

#define gc_vector_swap(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    if (__v->proc) { \
      TYPEOF(__v->proc) __tmp = __v->data; \
      size_t __tsize = __v->dsize; \
      __v->data = __v->proc; \
      __v->dsize = __v->psize; \
      __v->count = 0; \
      __v->psize = __tsize; \
      __v->proc = __tmp; \
    } \
  } while (0)

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

#define gc_vector_append(_g, _v, _value) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    if (!(__v->count < __v->dsize)) { \
      size_t __sz = ALIGN16(__v->count + 1); \
      __v->data = gc_mem_realloc(__g, __v->dsize, __sz * sizeof(__v->data[0]), __v->data); \
      __v->dsize = __sz; \
    } \
    __v->data[__v->count++] =  _value; \
  } while (0)

#define gc_vector_remove_all(_g, _v, _value) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_value) value = (_value); \
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
    TYPEOF(_l) __l = (_l); \
    __l->sweep = &__l->head; \
  } while (0)

#define gc_head_reset(_l) \
  do { \
    TYPEOF(_l) __l = (_l); \
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
    TYPEOF(_l) __l = (_l); \
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
    TYPEOF(_l) __l = (_l); \
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

#define rot(x, k) \
  (((x) << (k)) | ((x) >> (32 - (k))))

#define mix(a, b, c) \
  { \
    a -= c;  a ^= rot(c, 4);  c += b; \
    b -= a;  b ^= rot(a, 6);  a += c; \
    c -= b;  c ^= rot(b, 8);  b += a; \
    a -= c;  a ^= rot(c, 16);  c += b; \
    b -= a;  b ^= rot(a, 19);  a += c; \
    c -= b;  c ^= rot(b, 4);  b += a; \
  }

#define final(a, b, c) \
  { \
    c ^= b; c -= rot(b, 14); \
    a ^= c; a -= rot(c, 11); \
    b ^= a; b -= rot(a, 25); \
    c ^= b; c -= rot(b, 16); \
    a ^= c; a -= rot(c, 4); \
    b ^= a; b -= rot(a, 14); \
    c ^= b; c -= rot(b, 24); \
  }

inline static
uint32_t gc_hash(const void * key, size_t length, uint32_t initval)
{
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t) length) + initval;

  {
    const uint32_t * k = (const uint32_t *) key;

    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }
    if (length > 0) {
      uint32_t buff[3] = {0};
      memcpy(buff, k, length);
      a += buff[0];
      b += buff[1];
      c += buff[2];
      mix(a, b, c);
      final(a, b, c);
    }
  }

  return c;
}


inline static
void intern_obj(gc_global_t * g, gc_obj_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->objects.head), GC_HDR(o));
  g->total++;
}

inline static
void intern_hdr(gc_global_t * g, gc_hdr_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->headers.head), GC_HDR(o));
  g->total++;
}

inline static
void strings_resize(gc_global_t * g, gc_strings_t * ss, uint32_t newmask)
{
  log(0, "# %s(%08x->%08x)\n", __FUNCTION__, ss->mask, newmask);
  gc_buckets_t * buckets = &ss->buckets;

  gc_vector_process(g, buckets, newmask + 1);

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
}

inline static
void intern_str(gc_global_t * g, gc_str_t * s, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) s, size);
  GC_HDR(s)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->strings.buckets.data[s->hash & g->strings.mask].head), GC_HDR(s));
  if (g->strings.count++ > g->strings.mask) {
    uint32_t newmask = (g->strings.mask << 1) | 1;
    if (g->state != GC_STATE_SWEEP_STRING && newmask < 0xffffff - 1) {
      strings_resize(g, &g->strings, newmask);
    }
  }
}

extern gc_vtable_t gc_str_vtable;

#endif /* _GC_PRIVATE_H */
