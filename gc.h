#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

typedef struct gc_global gc_global_t;
typedef struct gc_header gc_header_t;
typedef struct gc_obj gc_obj_t;

#define bool int

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

#define gc_is_white(x)     ((x)->gc_flags & GC_WHITE_FLAGS)
#define gc_is_black(x)     ((x)->gc_flags & GC_BLACK_FLAG)
#define gc_is_grey(x)      (!((x)->gc_flags & GC_COLOR_FLAGS))
#define gc_swap_white(x)   ((uint16_t) ((x)->white ^ GC_WHITE_FLAGS))
#define gc_is_dead(x, y)   ((y)->gc_flags & gc_swap_white(x) & GC_WHITE_FLAGS)

#define gc_curr_white(x)   ((x)->white & GC_WHITE_FLAGS)
#define gc_new_white(x, y) ((y)->gc_flags = gc_curr_white(x))
#define gc_make_white(x, y) \
  ((y)->gc_flags = ((y)->gc_flags & (uint16_t) ~GC_COLOR_FLAGS) | gc_curr_white(x))
#define gc_flip_white(x)   ((y)->gc_flags ^= GC_WHITE_FLAGS)

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
  size_t          strsize;
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
