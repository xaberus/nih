#ifndef _NUMBER_H
#define _NUMBER_H

#include "gc/gc.h"

typedef struct {
  gc_hdr_t gch;
  int32_t  s;
  uint32_t data[];
} number_t;

number_t * number_new(gc_global_t * g, uint64_t bits);
number_t * number_setstrc(gc_global_t * g, number_t * n, size_t len, const char str[len]);
number_t * number_sethex(gc_global_t * g, number_t * n, gc_str_t * hex);

gc_str_t * number_gethex(gc_global_t * g, number_t * n);
gc_str_t * number_getdec(gc_global_t * g, number_t * n);

number_t * number_add(gc_global_t * g, number_t * a, number_t * b);
number_t * number_sub(gc_global_t * g, number_t * a, number_t * b);
number_t * number_mul(gc_global_t * g, number_t * a, number_t * b);
number_t * number_shl(gc_global_t * g, number_t * a, uint32_t s);
number_t * number_shr(gc_global_t * g, number_t * a, uint32_t s);

inline static
int number_get_u32(number_t * a, uint32_t * r)
{
  if (a->s == 1) {
    *r = *a->data;
    return 1;
  }
  return 0;
}

#endif /* _NUMBER_H */
