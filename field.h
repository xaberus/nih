#ifndef _FIELD_H
#define _FIELD_H

#include "gc.h"

enum field_vt_flags {
  FIELD_HDR_VT_FLAG = 0x08 | GC_VT_FLAG_HDR,
  FIELD_OBJ_VT_FLAG = 0x08 | GC_VT_FLAG_OBJ,
};

typedef struct {
  gc_vtable_t gcv;
  signed      (* fld_cmp)(gc_global_t * g, gc_hdr_t * o);
  unsigned    (* fld_le)(gc_global_t * g, gc_hdr_t * o);
  unsigned    (* fld_eq)(gc_global_t * g, gc_hdr_t * o);

  gc_hdr_t *  (* fld_keyget)(gc_global_t * g, gc_obj_t * o, gc_hdr_t * key);
  gc_hdr_t *  (* fld_keyset)(gc_global_t * g, gc_obj_t * o, gc_hdr_t * key, gc_hdr_t * value);
  gc_hdr_t *  (* fld_idxget)(gc_global_t * g, gc_obj_t * o, uint32_t idx);
  gc_hdr_t *  (* fld_idxset)(gc_global_t * g, gc_obj_t * o, uint32_t idx, gc_hdr_t * value);

  gc_hdr_t *  (* fld_cat)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  uint32_t    (* fld_len)(gc_global_t * g, gc_hdr_t * o);
  gc_hdr_t *  (* fld_add)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_sub)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_div)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_mod)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_mul)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_pow)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
  gc_hdr_t *  (* fld_unm)(gc_global_t * g, gc_hdr_t * o, gc_hdr_t * d);
} field_vtable_t;

/************************************************************************************************/
typedef struct {
  gc_hdr_t gch;
  int32_t  s;
  uint32_t data[];
} number_t;

number_t * number_new(gc_global_t * g, uint64_t bits);
number_t * number_sethexc(gc_global_t * g, number_t * n, size_t len, const char hex[len]);
number_t * number_sethex(gc_global_t * g, number_t * n, gc_str_t * hex);
gc_str_t * number_gethex(gc_global_t * g, number_t * n);
number_t * number_add(gc_global_t * g, number_t * a, number_t * b);
number_t * number_sub(gc_global_t * g, number_t * a, number_t * b);
/************************************************************************************************/

#endif /* _FIELD_H */
