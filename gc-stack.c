#include "gc-private.h"
#include "gc-stack.h"

#include <assert.h>

#define GC_STACK_MIN 16

static
size_t gc_stack_init(gc_global_t * g, gc_hdr_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;

  gc_vector_init_basic(g, s, GC_STACK_MIN);

  for (size_t k = 0; k < s->dsize; k++) {
    s->data[k] = NULL;
  }

  return 0;
}

static
size_t gc_stack_clear(gc_global_t * g, gc_hdr_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;

  gc_vector_clear_basic(g, s);

  return 0;
}

static
size_t gc_stack_propagate(gc_global_t * g, gc_obj_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;
  size_t       k, j;
  gc_hdr_t   * p;

  for (k = 0, j = 0; k < s->count; k++) {
    if ((p = s->data[k])) {
      gc_mark(g, p); j++;
    }
  }
  return j;
}

gc_vtable_t gc_stack_vtable = {
  .name = "gc_stack_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = gc_stack_init,
  .gc_finalize = NULL,
  .gc_clear = gc_stack_clear,
  .gc_propagate = gc_stack_propagate,
};

void gc_stack_set(gc_global_t * g, gc_stack_t * s, size_t len)
{
  gc_vector_resize(g, s, len);
}


void gc_stack_pushf(gc_global_t * g, gc_stack_t * s, gc_hdr_t * h)
{
  gc_vector_append(g, s, h);
  if (h) {
    gc_barrier(g, &s->gco, h);
  }
}

#define gc_stack_push(_g, _st, _d) \
  gc_stack_pushf(_g, _st, GC_HDR(_d))

#include "tests/gc-stack-tests.c"
