#include "gc/gc.h"
#include "gc/gc-stack.h"
#include "common/utf8.h"
#include "number/number.h"

enum sx_kind {
  SX_NONE  = 0x00,
  SX_LIST  = 0x01,
  SX_ATOM  = 0x02,
  SX_PLAIN = 0x04 | SX_ATOM,
  SX_SQ    = 0x08 | SX_ATOM,
  SX_DQ    = 0x10 | SX_ATOM,
  SX_NUM   = 0x20,
};

typedef struct sx sx_t;
struct sx {
  gc_obj_t gco;
  uint16_t kind;
  sx_t   * next;
  __extension__ union {
    sx_t     * lst;
    gc_str_t * str;
    number_t * num;
  };
};

typedef enum {
  SXBS_START = 0,
  SXBS_ERROR,
  SXBS_INTERMEDIATE,
  SXBS_COMMENT,
  SXBS_LIST_START,
  SXBS_LIST_END,
  SXBS_ATOM_START,
  SXBS_ATOM_END,
  SXBS_ESTRING,
    SXBS_ES_ESC,
    SXBS_ES_OCT, SXBS_ES_OCTW,
    SXBS_ES_HEX, SXBS_ES_HEXW,
    SXBS_ES_UNI, SXBS_ES_UNIW,
  SXBS_STRING,
  SXBS_EOB,
} sxbs_t;


typedef enum {
  SXBE_SUCCESS = 0,
  SXBE_UTF8,
} sxbe_t;

typedef struct {
  gc_hdr_t gco;
  sxbs_t       s, v;
  sxbe_t       e;
  const char * bp, * ba, * be, * b;
  ur_t         u;
  uint32_t     at;
  uint32_t     depth;
  uint32_t     line;
  uint32_t     aline;

  struct {
    uint32_t     b[4], l;
    uint32_t     s, ss;
    uint64_t     n;
    sxbs_t       t;
  } sg;
  struct {
    char * s, * sp, * se;
  } ag;

  sx_t       * cue;
  sx_t       * root, * last;
  gc_stack_t * stack;
} sxb_t;

/* tuple */ typedef struct { err_r * err; sxb_t * sxb; } e_sxb_t;
e_sxb_t sxb_new(gc_global_t * g);

/* tuple */ typedef struct { err_r * err; sx_t * sx; } e_sx_t;
e_sx_t sxb_read(gc_global_t * g, sxb_t * b, gc_str_t * str);

inline static
uint32_t sx_lnklen(sx_t * x)
{
  uint32_t len = 0;
  while (x) {
    len++;
    x = x->next;
  }
  return len;
}

e_gc_str_t sx_dump(gc_global_t * g, sx_t * x);
