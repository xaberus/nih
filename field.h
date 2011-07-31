#ifndef _FIELD_H
#define _FIELD_H

#include "gc.h"

#include "number.h"

/************************************************************************************************/
typedef struct keyval keyval_t;
struct keyval {
  gc_hdr_t * key;
  gc_hdr_t * val;
  uint32_t   hash;
  keyval_t * next;
  uint64_t   bits;
};
typedef struct {
  gc_obj_t gco;
  struct {
    struct {
      keyval_t ** data;
      uint32_t    dsize;
    } buckets;
    uint32_t    mask;
  } map;
  keyval_t * array;
} table_t;

table_t  * table_new(gc_global_t * g);
gc_hdr_t * table_get(gc_global_t * g, table_t * t, gc_hdr_t * k);
gc_hdr_t * table_set(gc_global_t * g, table_t * t, gc_hdr_t * k, gc_hdr_t * value);
gc_hdr_t * table_geti(gc_global_t * g, table_t * t, uint32_t i);
gc_hdr_t * table_seti(gc_global_t * g, table_t * t, uint32_t i, gc_hdr_t * value);
/************************************************************************************************/

/************************************************************************************************/
enum field_type {
  FIELD_NONE = 0,
  FIELD_UINT,
  FIELD_SEQ,
};
typedef struct field field_t;
struct field {
  gc_obj_t   gco;
  uint16_t   kind;
  struct {
    uint32_t width;
    uint32_t count;
    uint32_t alloc;
  } spec;
  struct {
    gc_str_t * name;
    field_t  * cld;
  } * cldn;
};
field_t  * field_new(gc_global_t * g, gc_str_t * def);
void       field_addf(gc_global_t * g, gc_str_t * name, field_t * f);
/************************************************************************************************/
#endif /* _FIELD_H */
