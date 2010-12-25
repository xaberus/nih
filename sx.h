#ifndef _SX_H
#define _SX_H

#include <stdint.h>
#include <malloc.h>

#include "err.h"
#include "sx-pool.h"

typedef struct sx sx_t;

#include "sx-strings.h"

union sx_atom {
  sx_str_t * string;
  uint64_t uint;
  int64_t sint;
};

#include "sx-utf8-decode.h"
#include "sx-utf8-encode.h"


struct sx {
  __extension__ struct {
    /* type */
    unsigned int islist : 1;
    unsigned int isatom : 1;

    unsigned int _reserved0 : 6;

    /* atom */
    unsigned int isplain : 1;
    unsigned int issquot : 1;
    unsigned int isdquot : 1;

    unsigned int issint : 1;
    unsigned int isuint : 1;
    unsigned int isdouble : 1;

    unsigned int _reserved1 : 2;
  };

  sx_t * next;
  unsigned int line;

  __extension__ union {
    sx_t * list;
    union sx_atom atom;
  };
};


#include "sx-stack.h"

enum sx_parser_error {
  SX_ERROR_SUCCESS = 0,
  SX_ERROR_PAREN_MISMATCH,
  SX_ERROR_MALFORMED_ATOM,
  SX_ERROR_UTF8_ERROR,
  SX_ERROR_LIST_START_GARBAGE,
  SX_ERROR_LIST_END_GARBAGE,
  SX_ERROR_ATOM_END_GARBAGE,
  SX_ERROR_UNHANDLED,
};

enum sx_parser_state {
  SX_PARSER_ERROR,
  SX_PARSER_INTERMEDIATE,
  SX_PARSER_COMMENT,
  SX_PARSER_LIST_START,
  SX_PARSER_LIST_END,
  SX_PARSER_ATOM_START,
  SX_PARSER_ATOM_END,
  SX_PARSER_ESTRING,
  SX_PARSER_STRING,
};

enum sx_atom_type {
  SX_ATOM_PLAIN,
  SX_ATOM_QUOTE,
  SX_ATOM_SQUOTE,
  SX_ATOM_DQUOTE,
  SX_ATOM_INT,
  SX_ATOM_UINT,
  SX_ATOM_FLOAT,
};


struct sx_ucr {
  unsigned int l : 3;
  unsigned int eob : 1;
  unsigned int err : 1;

  char n[7];
  uint32_t s;
  uint32_t c;
};

struct sx_parser {
#define FIELD(_type, _name) _type _name;
#include "sx-parser-fields.h"
#undef FIELD
  sx_pool_t pool[1];
  sx_strgen_t gen[1];
  sx_stack_t stack[1];

};

typedef struct sx_parser sx_parser_t;
typedef err_t (sx_event_function_t)(sx_parser_t *);

sx_parser_t * sx_parser_init(sx_parser_t * parser);
void          sx_parser_clear(sx_parser_t * parser);

err_t         sx_parser_atom_sq_fsm(sx_parser_t * parser, sx_str_t * str);
err_t         sx_parser_atom_dq_fsm(sx_parser_t * parser, sx_str_t * str);
err_t         sx_parser_atom_plain_fsm(sx_parser_t * parser, sx_t * sx, sx_str_t * str);
err_t         sx_parser_events(sx_parser_t * parser, sx_event_function_t * ef);

err_t         sx_parser_read(sx_parser_t * parser, const char * buffer, size_t length);

const char *  sx_parser_strerror(sx_parser_t * parser);

# ifdef TEST
int sx_test_print(sx_t * sx, unsigned int level, int len, unsigned int brk, int sl);
# endif /* TEST */

#endif /* _SX_H */

// vim: filetype=c:expandtab:shiftwidth=2:tabstop=4:softtabstop=2:encoding=utf-8:textwidth=100
