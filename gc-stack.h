#include "gc.h"

typedef struct {
  gc_obj_t      gco;
  gc_hdr_t   ** data;
  size_t        dsize;
  size_t        count;
} gc_stack_t;

