#include "gc.h"
#include "gc-stack.h"
#include "utf8.h"
#include "field.h"

enum sx_kind {
  SX_NONE  = 0x00,
  SX_LIST  = 0x01,
  SX_ATOM  = 0x02,
  SX_PLAIN = 0x04 | SX_ATOM,
  SX_SQ    = 0x08 | SX_ATOM,
  SX_DQ    = 0x10 | SX_ATOM,
  SX_NUM   = 0x20 | SX_ATOM,
};

typedef struct sx sx_t;
struct sx {
  gc_hdr_t gco;
  uint16_t kind;
  sx_t   * next;
  union {
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

sxb_t  * sxb_new(gc_global_t * g);
sx_t   * sxb_read(gc_global_t * g, sxb_t * b, size_t len, const char str[len]);
