#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

struct gc_global;

#define bool int

typedef size_t (gc_function_t)(struct gc_global * g, void * o);

struct gc_vtable {
  gc_function_t * gc_init;
  gc_function_t * gc_finalize;
  gc_function_t * gc_clear;
  gc_function_t * gc_propagate;
};

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
  struct gc_vtable * gc_vtable;  /* type information and gc_functions */ \
  size_t             gc_size; \
  struct gc_object * gc_next    /* global object list */ \

#define GC_OBJECT_FIELDS \
  GC_HEADER_FIELDS; \
  struct gc_object * gc_list    /* collection list  */ \

struct gc_header {
  GC_HEADER_FIELDS;
};

struct gc_string {
  GC_HEADER_FIELDS;
};

struct gc_object {
  GC_OBJECT_FIELDS;
};

enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
  GC_STATE_FINALIZE,
};

struct gc_global {
  struct gc_string ** strhash; /* note: array! */
  size_t              strmask;
  size_t              strsize;
  size_t              sweepstr;
  struct gc_object  * root; /* global object list */
  struct gc_header ** sweep;  /* global object list iterator */
  struct gc_object  * grey;   /* collection lists  */
  struct gc_object  * grey2;   /* collection lists  */
  struct gc_object  * fin;
  enum gc_state       state;
  struct gc_object ** gcroot; /* note: array! */
  size_t              gcroot_count;
  size_t              gcroot_size;
  size_t              threshold;
  size_t              total;
  size_t              stepmul;
  size_t              pause;
  size_t              estimate;
  size_t              debt;
  uint16_t            white; /* current white flag */
  mem_allocator_t     alloc;
};
