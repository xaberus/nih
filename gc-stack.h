#include "gc.h"

typedef struct {
  gc_obj_t      gco;
  gc_hdr_t   ** d;
  gc_hdr_t   ** dp;
  gc_hdr_t   ** de;
} gc_stack_t;

extern gc_vtable_t gc_stack_vtable;

void gc_stack_set(gc_global_t * g, gc_stack_t * s, size_t len);
void gc_stack_pushf(gc_global_t * g, gc_stack_t * s, gc_hdr_t * h);

#define gc_stack_push(_g, _st, _d) \
  gc_stack_pushf(_g, _st, GC_HDR(_d))

gc_hdr_t * gc_stack_top(gc_stack_t * s);
gc_hdr_t * gc_stack_pop(gc_stack_t * s);
size_t     gc_stack_size(gc_stack_t * s);

