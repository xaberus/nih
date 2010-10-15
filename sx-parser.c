#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <errno.h>

#include "sx.h"

static inline
int sx_is_whitespace(uint32_t uc)
{
  switch (uc) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\v':
      return 1;
    default:
      return 0;
  }
}

static inline
const char * sx_utf8_readon(const char * ba, const char * be, struct sx_ucr * ucr)
{
  struct sx_ucr u = *ucr;
  char          c;

  if (u.s == 1)
    goto utf8_error;

  if (u.s == 0)
    u.l = 0;

  if (ba < be) {
    while (ba < be) {
      c = *(ba++);
      if (sx_parser_decode_utf8(&u.s, &u.c, c) != 0) {
        if (u.s != 1) {
          u.n[u.l++] = c;
          continue;
        } else {
          goto utf8_error;
        }
      }
      u.n[u.l++] = c;
      break;
    }
  } else {
    u.eob = 1;
  }

  *ucr = u;

  return ba;
utf8_error:
  u.err = 1;
  *ucr = u;
  return NULL;
}

#define UTF8_READ(_var) \
  __extension__ \
    ({ \
      if (!(parser->ba = sx_utf8_readon(_var, parser->be, &parser->u))) \
              goto utf8_error; \
            if (parser->u.eob) \
              goto end_of_buffer; \
            if (parser->u.err) \
              goto utf8_error; \
            })

#define TRANS(_target, _state) \
  __extension__({ \
      parser->bp = _target; \
      parser->s = _state; \
      continue; \
    })


#define SETTRANS(_target, _state) \
  __extension__({ \
      parser->bp = _target; \
      parser->s = _state; \
    })

static inline
void sx_list_init(sx_t * sx)
{
  memset(sx, 0, sizeof(sx_t));
  sx->islist = 1;
}

static inline
void sx_list_set_list(sx_t * sx, sx_t * list)
{
  if (sx->islist)
    sx->list = list;
}

static inline
void sx_atom_clear(sx_t * sx, sx_parser_t * parser)
{
  if (sx->isatom) {
    if (sx->isplain || sx->issquot || sx->isdquot)
      sx_pool_retmem(parser->pool, sx->atom.string);

    memset(sx, 0, sizeof(sx_t));
  }
}

static inline
err_t sx_atom_init_string(sx_t * sx, sx_parser_t * parser, sx_str_t * str)
{
  memset(sx, 0, sizeof(sx_t));
  sx->isatom = 1;

  sx->line = parser->line;

  switch (parser->at) {
    case SX_ATOM_PLAIN:
      if (sx_parser_atom_plain_fsm(parser, sx, str).composite != 0)
        goto out;

      parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
      goto out;
    case SX_ATOM_QUOTE:
      if (parser->ss == '\"') {
        sx->isdquot = 1;


        if (sx_parser_atom_dq_fsm(parser, str).composite != 0)
          goto out;

        sx->atom.string = str;

        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
        goto out;
      } else if (parser->ss == '\'') {
        sx->issquot = 1;

        if (sx_parser_atom_sq_fsm(parser, str).composite != 0)
          goto out;

        sx->atom.string = str;
        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
        goto out;
      } else
        goto atom_quote_garbage;

atom_quote_garbage:
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, 255);
      goto out;
    default:
      sx_pool_retmem(parser->pool, str);
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, 255);
      goto out;
      break;
  }

out:
  return parser->err;
}

err_t sx_parser_events(sx_parser_t * parser, sx_event_function_t * ef)
{
  while (parser->bp < parser->be) {
    switch (parser->s) {
      case SX_PARSER_INTERMEDIATE:
        UTF8_READ(parser->bp);

        if (sx_is_whitespace(parser->u.c)) {
          if (parser->u.c == '\n')
            parser->line++;
          parser->bp = parser->ba;
          continue;
        } else if (parser->u.c == ';') {
          TRANS(parser->ba, SX_PARSER_COMMENT);
        }

        if (parser->u.c == '(') {
          TRANS(parser->bp, SX_PARSER_LIST_START);
        } else if (parser->u.c == ')') {
          TRANS(parser->bp, SX_PARSER_LIST_END);
        }

        TRANS(parser->bp, SX_PARSER_ATOM_START);
      case SX_PARSER_COMMENT:
        UTF8_READ(parser->bp);
        if (parser->u.c != '\n')
          TRANS(parser->ba, SX_PARSER_COMMENT);
        TRANS(parser->ba, SX_PARSER_INTERMEDIATE);
      case SX_PARSER_LIST_START:
        if (ef(parser).composite)
          goto error_in_event;
        parser->depth++;
        TRANS(parser->ba, SX_PARSER_INTERMEDIATE);
      case SX_PARSER_LIST_END:
        if (parser->depth == 0)
          goto paren_mismatch;
        parser->depth--;
        if (ef(parser).composite)
          goto error_in_event;
        TRANS(parser->ba, SX_PARSER_INTERMEDIATE);
      case SX_PARSER_ATOM_START:
        UTF8_READ(parser->bp);

        if (!sx_strgen_reset(parser->gen))
          goto out_of_memory;

        if (ef(parser).composite)
          goto error_in_event;

        if (parser->u.c == '"' || parser->u.c == '\'') {
          parser->ss = parser->u.c;
          parser->at = SX_ATOM_QUOTE;
          if (parser->u.l != sx_strgen_append(parser->gen, parser->u.n, parser->u.l))
            goto out_of_memory;
          SETTRANS(parser->ba, SX_PARSER_ESTRING);
        } else {
          parser->at = SX_ATOM_PLAIN;
          SETTRANS(parser->bp, SX_PARSER_STRING);
        }

        continue;
      case SX_PARSER_STRING:
        UTF8_READ(parser->bp);
        if (!sx_is_whitespace(parser->u.c)) {
          if (parser->u.c == ')')
            TRANS(parser->bp, SX_PARSER_ATOM_END);
          else if (parser->u.c == '"' || parser->u.c == '\'')
            goto malformed_atom;
          else if (parser->u.c >=32 /*&& parser->u.c <= 126*/) {
            if (parser->u.l != sx_strgen_append(parser->gen, parser->u.n, parser->u.l))
              goto out_of_memory;
            TRANS(parser->ba, SX_PARSER_STRING);
          }
          goto malformed_atom;
        }

        TRANS(parser->bp, SX_PARSER_ATOM_END);
      case SX_PARSER_ESTRING:
        UTF8_READ(parser->bp);

        if (parser->es == 0 && parser->u.c == parser->ss) {
          if (parser->u.l != sx_strgen_append(parser->gen, parser->u.n, parser->u.l))
            goto out_of_memory;
          TRANS(parser->ba, SX_PARSER_ATOM_END);
        }

        if (parser->u.c == '\\') {
          parser->es = 1;
          if (parser->u.l != sx_strgen_append(parser->gen, parser->u.n, parser->u.l))
            goto out_of_memory;
        } else {
          if (parser->u.c == '\n')
            parser->line++;

          if (parser->u.l != sx_strgen_append(parser->gen, parser->u.n, parser->u.l))
            goto out_of_memory;
          parser->es = 0;
        }

        TRANS(parser->ba, SX_PARSER_ESTRING);
      case SX_PARSER_ATOM_END:
        if (ef(parser).composite)
          goto error_in_event;
        TRANS(parser->bp, SX_PARSER_INTERMEDIATE);
      case SX_PARSER_ERROR:
        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_UNHANDLED);
    }
  }


end_of_buffer:
  parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
  goto out;
error_in_event:
  /* err is set by event handler */
  goto out;
paren_mismatch:
  parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_PAREN_MISMATCH);
  goto out;
malformed_atom:
  parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_MALFORMED_ATOM);
  goto out;
utf8_error:
  parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_UTF8_ERROR);
  goto out;
out_of_memory:
  parser->err = err_construct(ERR_MAJ_ALLOC, ERR_MIN_POOL_ALLOC, SX_ERROR_SUCCESS);
  goto out;
out:
  return parser->err;
}

err_t sx_parser_default_events(sx_parser_t * parser)
{
  sx_str_t * str = NULL;
  sx_t     * top = NULL;
  sx_t     * list = NULL;
  sx_t     * atom = NULL;

  switch (parser->s) {
    case SX_PARSER_LIST_START:
      list = sx_pool_getmem(parser->pool, sizeof(sx_t));
      if (!list)
        goto out_of_memory;
      sx_list_init(list);

      top = sx_stack_top(parser->stack);

      if (!sx_stack_push(parser->stack, list)) {
        sx_pool_retmem(parser->pool, list);
        goto out_of_memory;
      }

      if (parser->nsx) {
        parser->nsx->next = list;
        parser->nsx = NULL;
        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
        goto out;
      } else {
        if (top) {
          if (!top->islist) {
            sx_stack_pop(parser->stack);
            goto list_start_garbage;
          }
          top->list = list;
          parser->nsx = NULL;
          parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
          goto out;
        } else if (!parser->root) {
          parser->root = list;
          parser->nsx = NULL;
          parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
          goto out;
        }
        sx_stack_pop(parser->stack);
      }
list_start_garbage:
      sx_pool_retmem(parser->pool, list);
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_LIST_START_GARBAGE);
      goto out;
    case SX_PARSER_LIST_END:
      if ((top = sx_stack_top(parser->stack))) {
        if (!top->islist)
          goto list_end_garbage;
        sx_stack_pop(parser->stack);
        parser->nsx = top;
        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
        goto out;
      }
list_end_garbage:
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_LIST_END_GARBAGE);
      goto out;
    case SX_PARSER_ATOM_START:
      parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
      goto out;
    case SX_PARSER_ATOM_END:
      str = sx_strgen_get(parser->gen);
      if (!str)
        goto out_of_memory;

      atom = sx_pool_getmem(parser->pool, sizeof(sx_t));
      if (!atom) {
        sx_pool_retmem(parser->pool, str);
        goto out_of_memory;
      }

      if (sx_atom_init_string(atom, parser, str).composite)
        goto atom_end_garbage;

      if (parser->nsx) {
        parser->nsx->next = atom;
        parser->nsx = parser->nsx->next;
        parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
        goto out;
      } else {
        if ((top = sx_stack_top(parser->stack))) {
          if (!top->islist)
            goto atom_end_garbage;
          top->list = atom;
          parser->nsx = atom;
          parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
          goto out;
        } else if (!parser->root) {
          parser->root = atom;
          parser->nsx = atom;
          parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
          goto out;
        }
        goto atom_end_garbage;
      }
atom_end_garbage:
      sx_atom_clear(atom, parser);
      sx_pool_retmem(parser->pool, atom);
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_ATOM_END_GARBAGE);
      goto out;
    default:
      sx_atom_clear(atom, parser);
      sx_pool_retmem(parser->pool, atom);
      parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, 255);
      break;
  }
  parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_UNHANDLED);
  goto out;
out_of_memory:
  parser->err = err_construct(ERR_MAJ_ALLOC, ERR_MIN_POOL_ALLOC, SX_ERROR_SUCCESS);
  goto out;

out:
  return parser->err;
}

err_t sx_parser_read(sx_parser_t * parser, const char * buffer, size_t length)
{
  if (!buffer)
    return err_construct(ERR_MAJ_NULL_POINTER, ERR_MIN_IN_INVALID, SX_ERROR_SUCCESS);

  parser->b = buffer;
  parser->bp = parser->b;
  parser->be = parser->b + length;

  parser->u.eob = 0;

  return sx_parser_events(parser, sx_parser_default_events);
}

const char * sx_parser_strerror(sx_parser_t * parser)
{
  enum sx_parser_error error = parser->err.extra;

  switch (error) {
    case SX_ERROR_SUCCESS:
      return "success";
    case SX_ERROR_PAREN_MISMATCH:
      return "cannot close parenthesis, which was never opened";
    case SX_ERROR_MALFORMED_ATOM:
      return "malformed atom";
    case SX_ERROR_UTF8_ERROR:
      return "input contains invalid utf-8 string";
    case SX_ERROR_LIST_START_GARBAGE:
      return "garbage at start of list";
    case SX_ERROR_LIST_END_GARBAGE:
      return "garbage at end of list";
    case SX_ERROR_ATOM_END_GARBAGE:
      return "garbage at end of atom";
    case SX_ERROR_UNHANDLED:
      return "the parser was not able to handle given input";
  }
  return NULL;
}

static inline
sx_t * sx_get_last(sx_t * sx)
{
  while (sx && sx->next)
    sx = sx->next;

  return sx;
}

void sx_parser_free_nodes(sx_parser_t * parser, sx_t * sx)
{
  /*
   *  assertion: no stack tracher needed!
   *  assumptions:
   *  - top level list is shorter than bottom level
   *  - o(n) performance on rach level is still better than stack overflows
   */
  sx_t * f;
  sx_t * l;

  while (sx) {
    f = sx; sx = sx->next;

    if (f->isatom)
      sx_atom_clear(f, parser);
    else if (f->islist) {
      l = sx_get_last(f);
      if (l != f)
        l->next = f->list;
      else
        sx = f->list;
    }

    sx_pool_retmem(parser->pool, f);
  }
  /* q.e.d */
}


sx_parser_t * sx_parser_init(sx_parser_t * parser)
{
  if (!parser)
    return NULL;

  memset(parser, 0, sizeof(sx_parser_t));

  parser->s = SX_PARSER_INTERMEDIATE;
  parser->line = 1;


  if (!sx_pool_init(parser->pool))
    return NULL;

  if (!sx_stack_init(parser->pool, parser->stack)) {
    sx_pool_clear(parser->pool);
    return NULL;
  }

  if (!sx_strgen_init(parser->pool, parser->gen)) {
    sx_stack_clear(parser->stack);
    sx_pool_clear(parser->pool);
    return NULL;
  }

  return parser;
}

void sx_parser_clear(sx_parser_t * parser)
{
  sx_parser_free_nodes(parser, parser->root);
  sx_strgen_clear(parser->gen);
  sx_stack_clear(parser->stack);
  sx_pool_clear(parser->pool);
}

#include "tests/sx-parser-tests.c"
