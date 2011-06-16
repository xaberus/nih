#include "gc-stack.h"

#define GC_STACK_MIN 16

size_t gc_stack_init(gc_global_t * g, void * o)
{
  gc_stack_t * s = o;

  s->g = g;
  s->s = gc_mem_newvec(g, GC_STACK_MIN, gc_hdr_t *);
  s->st = 0;
  s->sz = GC_STACK_MIN;
  for (size_t k = 0; k < s->sz; k++) {
    s->s[k] = NULL;
  }

  return sizeof(gc_stack_t) + sizeof(gc_hdr_t *) * s->sz;
}

size_t gc_stack_clear(gc_global_t * g, void * o)
{
  gc_stack_t * s = o;

  gc_mem_freevec(g, s->sz, gc_hdr_t *, s->s);

  return sizeof(gc_stack_t) + sizeof(gc_hdr_t *) * s->sz;
}

size_t gc_stack_propagate(gc_global_t * g, void * o)
{
  gc_stack_t * s = o;
  size_t       k, j;

  for (k = 0, j = 0; k < s->st; k++) {
    gc_hdr_t * h;

    if ((h = s->s[k])) {
      if (gc_is_white(h)) {
        gc_mark(g, h); j++;
      }
    }
  }
  return j;
}

gc_vtable_t gc_stack_vtable = {
  .gc_init = gc_stack_init,
  .gc_finalize = NULL,
  .gc_clear = gc_stack_clear,
  .gc_propagate = gc_stack_propagate,
};

void gc_stack_set(gc_stack_t * s, size_t len)
{
  if (s->sz < len) {
    gc_mem_growvec(s->g, s->sz, len, gc_hdr_t *, s->s);
    for (size_t k = s->st; k < s->sz; k++) {
      s->s[k] = NULL;
    }
  }

  if (len < s->st) {
    s->st = len;
  }
}


void gc_stack_push(gc_stack_t * s, gc_hdr_t * h)
{
  gc_stack_set(s, s->st + 1);
  s->s[s->st++] = h;
}



#include "tests/gc-stack-tests.c"
