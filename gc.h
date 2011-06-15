#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

typedef struct gc_global gc_global_t;
typedef struct gc_header gc_header_t;
typedef struct gc_obj gc_obj_t;

typedef size_t (gc_function_t)(gc_global_t * g, void * o);

typedef struct gc_vtable {
  gc_function_t * gc_init;
  gc_function_t * gc_finalize;
  gc_function_t * gc_clear;
  gc_function_t * gc_propagate;
} gc_vtable_t;

#define GC_WHITE0_FLAG    0x01
#define GC_WHITE1_FLAG    0x02
#define GC_BLACK_FLAG     0x04
#define GC_FINALIZED_FLAG 0x08
#define GC_FIXED_FLAG     0x10
#define GC_SFIXED_FLAG    0x20

#define GC_WHITE_FLAGS    (GC_WHITE0_FLAG | GC_WHITE1_FLAG)
#define GC_COLOR_FLAGS    (GC_WHITE_FLAGS | GC_BLACK_FLAG)

#define gc_is_white(x) ((x)->gc_flags & GC_WHITE_FLAGS)
#define gc_is_black(x) ((x)->gc_flags & GC_BLACK_FLAG)

#define GC_HEADER_FIELDS \
  uint16_t gc_flags; \
  gc_vtable_t * gc_vtable;  /* type information and gc_functions */ \
  size_t        gc_size; \
  gc_header_t * gc_next    /* global object list */ \

#define GC_OBJECT_FIELDS \
  GC_HEADER_FIELDS; \
  gc_obj_t    * gc_list    /* collection list  */ \

struct gc_header {
  GC_HEADER_FIELDS;
};

typedef struct gc_str {
  GC_HEADER_FIELDS;
  uint16_t id;
  size_t   hash;
  size_t   len;
  char     data[];
} gc_str_t;

struct gc_obj {
  GC_OBJECT_FIELDS;
};

typedef enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
  GC_STATE_FINALIZE,
} gc_state_t;

struct gc_global {
  gc_str_t     ** strhash; /* note: array! */
  size_t          strmask;
  size_t          strcount;
  size_t          sweepstr;
  gc_header_t   * root; /* global object list */
  gc_header_t  ** sweep;  /* global object list iterator */
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
  uint16_t        white; /* current white flag */
  mem_allocator_t alloc;
};

gc_global_t * gc_global_init(gc_global_t * g, mem_allocator_t alloc);
void          gc_global_clear(gc_global_t * g);
void          gc_full_gc(gc_global_t * g);

gc_str_t *    gc_mem_new_str(gc_global_t * g, const char * str, size_t len);
void *        gc_mem_new_obj(gc_global_t * g, gc_vtable_t * vtable, size_t size);

void          gc_add_root_obj(gc_global_t * g, gc_obj_t * o);
void          gc_del_root_obj(gc_global_t * g, gc_obj_t * o);

void          gc_barrierback(gc_global_t * g, gc_obj_t * o);
#define gc_obj_barriert(L, t, o) \
  do { \
    if (gc_is_white(o) && gc_is_black(t)) { \
      gc_barrierback(g, t); \
    } \
  } while (0)

void          gc_mark(gc_global_t * g, gc_obj_t * o);
#define gc_mark_obj(x, y) \
  do { \
    if (gc_is_white(y)) \
      gc_mark(g, y); \
  } while (0)

int           gc_step(gc_global_t * g);
#define gc_check(g) \
  do { \
    if ((g)->total >= g->threshold) { \
      gc_step(L); \
    } \
  } while (0)

