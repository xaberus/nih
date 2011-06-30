#include "gc.h"

#define GC_STACK_FIELDS \
  GC_OBJ_FIELDS(gc_stack_t) \
  gc_global_t * g; \
  gc_hdr_t   ** s;  /* stack */ \
  size_t        sz; /* size */ \
  size_t        st;  /* top */ \

#define GC_STACK_UNION_ADD \
  gc_hdr_t gch; \
  gc_obj_t gco;

GC_PARTDEF(gc_stack, GC_STACK_FIELDS, gst, GC_STACK_UNION_ADD);
