#include "gc-stack.h"
#include "tests/gc-stack-tests.c"

#include <assert.h>

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

#define GC_STACK_MIN 16

static
size_t gc_stack_init(gc_global_t * g, gc_hdr_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;

  s->d = gc_mem_new(g, sizeof(gc_hdr_t *) * GC_STACK_MIN);
  s->de = s->d + GC_STACK_MIN;

  s->dp = NULL; /* empty */

  return 0;
}

static
size_t gc_stack_clear(gc_global_t * g, gc_hdr_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;

  gc_mem_free(g, s->de - s->d, s->d);

  return 0;
}

static
size_t gc_stack_propagate(gc_global_t * g, gc_obj_t * o)
{
  gc_stack_t * s = (gc_stack_t *) o;
  size_t       k = 0;
  gc_hdr_t   * p, ** c;

  if (s->dp) {
    for (c = s->d; c <= s->dp; c++) {
      if ((p = *c)) {
        gc_mark(g, p); k++;
      }
    }
  }
  return k;
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
  gc_hdr_t ** d;
  size_t alloc = s->de - s->d;

  //fprintf(stderr, "SET: len:%+4lu [%+8p|%+8p|%+8p]\n", len, s->d, s->dp, s->de);

  if (s->dp && (size_t) (s->dp -s->d) > len) {
    if (len) {
      s->dp = s->d + len;
    } else {
      s->dp = NULL;
    }
  }

  if (len > alloc) {
    len = ALIGN16(len);
    d = gc_mem_realloc(g, alloc * sizeof(gc_stack_t), len * sizeof(gc_stack_t), s->d);
    if (s->dp) {
      s->dp = d + (s->dp - s->d);
    }
    s->d = d;
    s->de = d + len;
  }
  //fprintf(stderr, "       ->     [%+8p|%+8p|%+8p]\n", s->d, s->dp, s->de);
}


void gc_stack_pushf(gc_global_t * g, gc_stack_t * s, gc_hdr_t * h)
{
  gc_hdr_t ** d;
  size_t alloc, len;

  if (!s->dp) {
    s->dp = s->d;
    *(s->dp) = h;
  } else {
    if (++s->dp < s->de) {
      *(s->dp) = h;
    } else {
      alloc = (s->de - s->d);
      len = ALIGN16(alloc + 1);
      d = gc_mem_realloc(g, alloc * sizeof(gc_stack_t), len * sizeof(gc_stack_t), s->d);
      s->dp = d + (s->dp - s->d);
      s->d = d;
      s->de = d + len;
      *(s->dp) = h;
    }
  }
  if (h) {
    gc_barrier(g, &s->gco, h);
  }
}

gc_hdr_t * gc_stack_top(gc_stack_t * s)
{
  return s->dp ? *(s->dp) : NULL;
}

gc_hdr_t * gc_stack_pop(gc_stack_t * s)
{
  gc_hdr_t * r = gc_stack_top(s);
  if (s->dp && --s->dp < s->d) {
    s->dp = NULL;
  }
  return r;
}

size_t gc_stack_size(gc_stack_t * s)
{
  if (s->dp) {
    return s->dp - s->d + 1;
  }
  return 0;
}
