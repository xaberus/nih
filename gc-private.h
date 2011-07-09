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

#define gc_vector_reset(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
    memset(__v->proc, 0, sizeof(TYPEOF(__v->proc[0])) * __v->psize); \
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

#endif /* _GC_PRIVATE_H */
