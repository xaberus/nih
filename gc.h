#ifndef _GC_H
#define _GC_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#define GC_WHITE0_FLAG    0x01
#define GC_WHITE1_FLAG    0x02
#define GC_BLACK_FLAG     0x04
#define GC_FINALIZED_FLAG 0x08
#define GC_FIXED_FLAG     0x10
#define GC_SFIXED_FLAG    0x20

#define GC_WHITE_FLAGS    (GC_WHITE0_FLAG | GC_WHITE1_FLAG)
#define GC_COLOR_FLAGS    (GC_WHITE_FLAGS | GC_BLACK_FLAG)

#define gc_is_white(x)   ((x)->_flags & GC_WHITE_FLAGS)
#define gc_is_black(x)   ((x)->_flags & GC_BLACK_FLAG)
#define gc_white2gray(x) ((x)->_flags &= (uint16_t) ~GC_WHITE_FLAGS)

#define GC_PARTDEF(__name, __fields, __acc, ...)\
  typedef union __name __name ## _t; \
  union __name { \
    struct { \
      __fields \
    } __acc; \
    __extension__ struct { \
      __fields \
    }; \
    __VA_ARGS__ \
  }

typedef struct gc_vtable gc_vtable_t;
typedef struct gc_global gc_global_t;

#define GC_HEADER_FIELDS(__base_t) \
  /* type information and gc_functions */ \
  gc_vtable_t * _vtable; \
  uint16_t      _flags; \
  size_t        _size; \
  /* global object list */ \
  __base_t    * _next;

#define GC_EMPTY_UNION_ADD

GC_PARTDEF(gc_hdr, GC_HEADER_FIELDS(gc_hdr_t), gch, GC_EMPTY_UNION_ADD);

#define GC_HEADER_UNION_ADD \
  gc_hdr_t gch;

#define GC_OBJ_FIELDS(__base_t) \
  GC_HEADER_FIELDS(__base_t) \
  /* collection list */ \
  gc_obj_t * _list;

GC_PARTDEF(gc_obj, GC_OBJ_FIELDS(gc_obj_t), gco, GC_HEADER_UNION_ADD);

#define GC_OBJ_UNION_ADD \
  gc_hdr_t gch; \
  gc_obj_t o;

#define GC_STR_FIELDS \
  GC_HEADER_FIELDS(gc_str_t) \
  uint16_t id; \
  size_t   hash; \
  size_t   len; \
  char     data[];

#define GC_STR_UNION_ADD \
  gc_hdr_t gch;

GC_PARTDEF(gc_str, GC_STR_FIELDS, gcs, GC_STR_UNION_ADD);
extern gc_vtable_t gc_str_vtable;

typedef enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
  GC_STATE_FINALIZE,
} gc_state_t;

struct gc_global {
  mem_allocator_t alloc;
  uint16_t        white; /* current white flag */
  gc_str_t     ** strhash; /* note: array! */
  size_t          strmask;
  size_t          strcount;
  size_t          sweepstr;
  gc_hdr_t      * root; /* global object list */
  gc_hdr_t     ** sweep;  /* global object list iterator */
  gc_obj_t      * grey;   /* collection lists  */
  gc_obj_t      * grey2;   /* collection lists  */
  gc_obj_t      * fin;
  gc_obj_t     ** gcroot; /* note: array! */
  size_t          gcroot_count;
  size_t          gcroot_size;
  gc_state_t      state;
  size_t          threshold;
  size_t          total;
  size_t          stepmul;
  size_t          pause;
  size_t          estimate;
  size_t          debt;
};

typedef size_t (gc_function_t)(gc_global_t * g, void * o);
struct gc_vtable {
  gc_function_t * gc_init;
  gc_function_t * gc_clear;
  gc_function_t * gc_propagate;
  gc_function_t * gc_finalize;
};


gc_global_t * gc_global_init(gc_global_t * g, mem_allocator_t alloc);
void          gc_global_clear(gc_global_t * g);
void          gc_full_gc(gc_global_t * g);
gc_str_t *    gc_mem_new_str(gc_global_t * g, const char * str, size_t len);
void *        gc_mem_new_obj(gc_global_t * g, gc_vtable_t * vtable, size_t size);
void          gc_add_root_obj(gc_global_t * g, gc_obj_t * o);
void          gc_del_root_obj(gc_global_t * g, gc_obj_t * o);
void          gc_barrierf(gc_global_t * g, gc_obj_t * o, gc_hdr_t * v);
void          gc_barrierbackf(gc_global_t * g, gc_obj_t * o);
void          gc_markf(gc_global_t * g, gc_hdr_t * o);
int           gc_step(gc_global_t * g);
void *        gc_mem_realloc(gc_global_t * g, size_t osz, size_t nsz, void * p);
void *        gc_mem_grow(gc_global_t * g, size_t * szp, size_t lim, size_t esz, void * p);
void          gc_mem_free(gc_global_t * g, size_t size, void * p);

#define gc_barrier(g, p, v) \
  do { \
    if (gc_is_white(v) && gc_is_black(p)) { \
      gc_barrierf(g, &(p)->gco, &(v)->gch); \
    } \
  } while (0)

#define gc_barrierback(g, t, o) \
  do { \
    if (gc_is_white(o) && gc_is_black(t)) { \
      gc_barrierbackf(g, &(t)->gco); \
    } \
  } while (0)

#define gc_mark(x, y) \
  do { \
    if (gc_is_white(y)) \
      gc_markf(x, &(y)->gch); \
  } while (0)

#define gc_markh(x, y) \
  do { \
    if (gc_is_white(y)) \
      gc_markf(x, y); \
  } while (0)

#define gc_check(x) \
  do { \
    if ((x)->total >= x->threshold) { \
      gc_step(x); \
    } \
  } while (0)

#define gc_mem_new(g, s) \
  gc_mem_realloc(g, 0, (s), NULL)
#define gc_mem_newvec(g, n, t) \
  ((t *) gc_mem_new(g, (size_t) ((n) * sizeof(t))))
#define gc_mem_growvec(g, n, m, t, p) \
  ((p) = (t *) gc_mem_grow(g, &(n), (m), sizeof(t), (p)))
#define gc_mem_freevec(g, n, t, p) \
  gc_mem_free(g, (n) * sizeof(t), (p))

#endif /* _GC_H */
