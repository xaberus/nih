#include <stdio.h>

#include "sx.h"

static
err_r * sx_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  sx_t * x = (sx_t *) o;
  (void) g;
  (void) argc;
  (void) ap;

  x->kind = SX_NONE;
  x->next = NULL;

  return NULL;
}
static
size_t sx_propagate(gc_global_t * g, gc_obj_t * o)
{
  sx_t * x = (sx_t *) o;

  if (x->kind == SX_LIST) {
    if (x->lst) {
      gc_mark_obj(g, GC_OBJ(x->lst));
      return 1;
    }
  } else if (x->kind == SX_ATOM) {
    gc_mark_str(g, x->str);
    return 1;
  } else if (x->kind == SX_NUM) {
    gc_mark(g, GC_HDR(x->num));
    return 1;
  }

  return 0;
}

gc_vtable_t sx_vtable = {
  .name = "sx_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = sx_init,
  .gc_clear = NULL,
  .gc_propagate = sx_propagate,
};

typedef struct {
  uint32_t level;
} wrec_t;

inline static
e_gc_str_t levelstr(gc_global_t * g, uint32_t level)
{
  char buf[level];
  memset(buf, ' ', level);
  return gc_new_str(g, level, buf);
}

inline static
uint32_t iabs(int32_t i)
{
  return i > 0 ? i : -i;
}

inline static
uint32_t atom_len(sx_t * a)
{
  assert(a->kind & (SX_ATOM | SX_NUM));
  uint32_t l = 0;
  switch (a->kind) {
    case SX_SQ:
    case SX_DQ:
      l += 2;
    case SX_PLAIN:
      l += gc_str_len(a->str);
      break;
      return l;
    /*case SX_NUM:
      l += iabs(a->num->s) * 9;
      return l;*/
  }
  return l;
}

inline static
uint32_t istail(sx_t * x)
{
  uint32_t c = 0, l = 0;
  for (; x; x = x->next) {
    if (!(x->kind & SX_ATOM)) {
      return 0;
    }
    l += atom_len(x);
    c++;
  }
  return (l < 10) ? c: 0;
}


inline static
uint32_t isshortlist(sx_t * x)
{
  assert(x->kind & SX_LIST);
  uint32_t c = 0, l = 0;
  for (sx_t * s = x->lst; s; s = s->next) {
    if (!(s->kind & (SX_ATOM | SX_NUM))) {
      if ((s->kind & SX_LIST)) {
        if (s->lst == NULL) {
          l += 2;
          c++;
          continue;
        } else if(!s->lst->next) {
          l += 20;
          c++;
          continue;
        }
      }
      return 1;
    }
    l += atom_len(s);
    c++;
  }
  if (l > 80) {
    return 0;
  }

  if (c >= 2) {
    return (l < 40) ? c : 1;
  } else {
    return (l < 30) ? 1 : 0;
  }
}

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

err_r * sx_writer(gc_global_t * g, gc_stack_t * st, sx_t * x, wrec_t * wr)
{
  err_r * err = NULL;
  uint32_t lva = 0;
  uint32_t lvl = 0;
  struct {
    sx_t   * sx;
    uint32_t indent;
    uint32_t shortlist;
  } * lvv = NULL;
  uint32_t indent = 0;
  uint32_t lastindent = 0;
  uint32_t shortlist = 1;

#define lwrite_lit(_s) \
  do { \
    e_gc_str_t e = gc_new_str(g, sizeof(_s) - 1, (_s)); \
    if (e.err) { \
      err = err_return(ERR_FAILURE, "gc_new_str() failed"); \
      goto out; \
    } \
    if (gc_stack_push(g, st, e.gc_str)) { \
      err = err_return(ERR_FAILURE, "gc_stack_push() failed"); \
      goto out; \
    } \
  } while(0)

#define lwrite_gc_str(_s) \
  do { \
    if (gc_stack_push(g, st, (_s))) { \
      err = err_return(ERR_FAILURE, "gc_stack_push() failed"); \
      goto out; \
    } \
  } while(0)

#define lwrite_indent(_i) \
  do { \
    e_gc_str_t e = levelstr(g, (_i)); \
    if (e.err) { \
      err = err_return(ERR_FAILURE, "gc_new_str() failed"); \
      goto out; \
    } \
    if (gc_stack_push(g, st, e.gc_str)) { \
      err = err_return(ERR_FAILURE, "gc_stack_push() failed"); \
      goto out; \
    } \
  } while(0)

  while(x) {
    switch (x->kind) {
      case SX_PLAIN:
        lastindent = indent;
        lwrite_gc_str(x->str);
        indent += gc_str_len(x->str);
        break;
      case SX_SQ:
        lastindent = indent;
        lwrite_lit("'"); indent++;
        lwrite_gc_str(x->str); indent += gc_str_len(x->str);
        lwrite_lit("'"); indent++;
        break;
      case SX_DQ:
        lastindent = indent;
        lwrite_lit("\""); indent++;
        lwrite_gc_str(x->str); indent += gc_str_len(x->str);
        lwrite_lit("\""); indent++;
        break;
      case SX_NUM: {
        lastindent = indent;
        e_gc_str_t e = number_getdec(g, x->num);
        if (e.err) {
          err = err_return(ERR_FAILURE, "number_getdec() failed");
          goto out;
        }
        lwrite_gc_str(e.gc_str); indent += gc_str_len(e.gc_str);
        break;
      }
      case SX_NONE:
      case SX_ATOM:
        err = err_return(ERR_IN_INVALID, "incomplete sx tuple passed");
        goto out;
      case SX_LIST:
        if (x->lst) {
          if (lvl >= lva) {
            lva = ALIGN16(lvl + 1);
            void * tmp = realloc(lvv, lva * sizeof(lvv[0]));
            if (!tmp) {
              err = err_return(ERR_MEM_REALLOC, "realloc() failed");
              goto out;
            }
            lvv = tmp;
          }
          lastindent = indent;
          lvv[lvl].sx = x; lvv[lvl].indent = indent; lvv[lvl].shortlist = shortlist; lvl++;
          lwrite_lit("("); indent++;
          shortlist = isshortlist(x);
          x = x->lst;
          continue;
        } else {
          lastindent = indent;
          lwrite_lit("()"); indent += 2;
        }
    }

    x = x->next;
    if (x) {
      if (!shortlist) {
        shortlist = istail(x);
      }
      if (shortlist) {
        shortlist--;
        lwrite_lit(" "); indent++;
      } else {
        lwrite_lit("\n");
        indent = lastindent;
        lwrite_indent(indent);
      }
    } else {
      uint32_t locindent = 0;
      while (!x && lvl) {
        lvl--; x = lvv[lvl].sx->next; locindent = lvv[lvl].indent; shortlist = lvv[lvl].shortlist;
        lwrite_lit(")"); indent++;
      }
      if (shortlist) {
        shortlist--;
        lwrite_lit(" "); indent++;
      } else {
        lastindent = indent = locindent;
        lwrite_lit("\n");
        lwrite_indent(indent);
      }
    }
  }

out:
  free(lvv);

  return err;
}

//e_uint32_t sx_writer(gc_global_t * g, gc_stack_t * st, sx_t * x, wrec_t * wr);
/*{
  switch (x->kind) {
    case SX_LIST: {
      uint32_t level = wr->level;
      if (gc_stack_push(g, st, gc_new_str(g, 1, "("))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      wr->level++;
      sx_t * c = x->lst;

      if (!isatomlist(x)) {
        if (c) {
          wr->level = sx_writer(g, st, c, wr);
          c = c->next;
        }

        if (c) {
          if (gc_stack_push(g, st, gc_new_str(g, 1, " "))) {
            return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
          }
          wr->level++;
          wr->level = sx_writer(g, st, c, wr);
          c = c->next;
        }

        while (c) {
          if ((c->kind & SX_ATOM)) {
            if (gc_stack_push(g, st, gc_new_str(g, 1, " "))) {
              return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
            }
            wr->level++;
            wr->level = sx_writer(g, st, c, wr);
          } else {
            if ((c->kind & SX_LIST) && !c->lst) {
              if (gc_stack_push(g, st, gc_new_str(g, 1, " "))) {
                return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
              }
              if (gc_stack_push(g, st, gc_new_str(g, 2, "()"))) {
                return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
              }
              wr->level += 3;
            } else {
              if (gc_stack_push(g, st, gc_new_str(g, 1, "\n"))) {
                return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
              }
              if (gc_stack_push(g, st, levelstr(g, wr->level))) {
                return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
              }
              wr->level = sx_writer(g, st, c, wr);
            }
          }
          c = c->next;
        }
      } else {
        while (c) {
          if (gc_stack_push(g, st, gc_new_str(g, 1, "\n"))) {
            return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
          }
          if (gc_stack_push(g, st, levelstr(g, wr->level))) {
            return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
          }
          sx_writer(g, st, c, wr);
          c = c->next;
        }
      }
      if (gc_stack_push(g, st, gc_new_str(g, 1, ")"))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      wr->level = level;
      return level;
      break;
    }
    case SX_PLAIN:
      if (gc_stack_push(g, st, x->str)) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      return wr->level + gc_str_len(x->str);
    case SX_SQ:
      if (gc_stack_push(g, st, gc_new_str(g, 1, "'"))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      if (gc_stack_push(g, st, x->str)) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      if (gc_stack_push(g, st, gc_new_str(g, 1, "'"))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      return wr->level + gc_str_len(x->str) + 2;
    case SX_DQ:
      if (gc_stack_push(g, st, gc_new_str(g, 1, "\""))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      if (gc_stack_push(g, st, x->str)) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      if (gc_stack_push(g, st, gc_new_str(g, 1, "\""))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      return wr->level + gc_str_len(x->str) + 2;
    case SX_NUM: {
      if (gc_stack_push(g, st, number_getdec(g, x->num))) {
        return (e_uint32_t) {err_return(ERR_FAILURE, "gc_stack_push() failed"), 0};
      }
      return wr->level + gc_str_len((gc_str_t *) gc_stack_top(st));
    }
    case SX_NONE:
    case SX_ATOM:
      fprintf(stderr, "error! (atom type %x)\n", x->kind);
      assert(0);
      break;
  }
  return 0;
}*/

e_gc_str_t sx_dump(gc_global_t * g, sx_t * x)
{
  gc_stack_t * st;
  wrec_t       wr = {0};

  {
    e_void_t e = gc_new(g, &gc_stack_vtable, sizeof(gc_stack_t), 0);
    if (e.err) {
      return (e_gc_str_t) {err_return(ERR_FAILURE, "could not create a new gc-stack"), NULL};
    }
    st = e.value;
  }

  if (sx_writer(g, st, x, &wr)) {
    return (e_gc_str_t) {err_return(ERR_FAILURE, "sx_writer() failed"), NULL};
  }

  return gc_stack_strcat(g, st);
}

/**************************************************/

typedef err_r * (sxb_evfn_t)(gc_global_t * g, sxb_t * b);

typedef __typeof__(((sxb_t *) NULL)->ag) ag_t;
typedef __typeof__(((sxb_t *) NULL)->sg) sg_t;

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

err_r * ag_append(ag_t * a, size_t len, const char str[len])
{
  size_t alloc = a->se - a->s;
  size_t rest = a->se - a->sp;

  if (!a->s) {
    rest = alloc = ALIGN16(len);
    a->s = a->sp = malloc(alloc);
    if (!a->s) {
      return err_return(ERR_MEM_ALLOC, "malloc() failed");
    }
    a->se = a->s + alloc;
  } else if (rest < len) {
    size_t o = a->sp - a->s;
    size_t n = alloc << 1;
    void * tmp = realloc(a->s, n);
    if (!tmp) {
      return err_return(ERR_MEM_REALLOC, "realloc() failed");
    }
    a->s = tmp;
    a->sp = a->s + o;
    a->se = a->s + n;
  }

  memcpy(a->sp, str, len);

  a->sp += len;

  return NULL;
}

static
err_r * sxb_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  sxb_t * b = (sxb_t *) o;
  (void) g;
  (void) argc;
  (void) ap;

  b->s = SXBS_START;
  b->e = SXBE_SUCCESS;
  b->b = b->bp = b->ba = b->be = NULL;
  memset(&b->u, 0, sizeof(ur_t));
  b->at = SX_NONE;
  b->depth = 0;
  b->line = 1;
  b->aline = 1;
  memset(&b->sg, 0, sizeof(sg_t));
  memset(&b->ag, 0, sizeof(ag_t));

  b->last = b->root = NULL;
  b->cue = NULL;
  {
    e_void_t e = gc_new(g, &gc_stack_vtable, sizeof(gc_stack_t), 0);
    if (e.err) {
      return err_return(ERR_FAILURE, "gc_new() failed");
    }
    b->stack = e.value;
  }

  return NULL;
}

static
size_t sxb_clear(gc_global_t * g, gc_hdr_t * o)
{
  (void) g;
  sxb_t * b = (sxb_t *) o;

  if (b->ag.s) {
    free(b->ag.s);
  }

  return 0;
}

static
size_t sxb_propagate(gc_global_t * g, gc_obj_t * o)
{
  sxb_t * s = (sxb_t *) o;

  gc_mark_obj(g, GC_OBJ(s->stack));

  return 0;
}

gc_vtable_t sxb_vtable = {
  .name = "sxb_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = sxb_init,
  .gc_clear = sxb_clear,
  .gc_propagate = sxb_propagate,
};



e_sxb_t sxb_new(gc_global_t * g)
{
  e_void_t e = gc_new(g, &sxb_vtable, sizeof(sxb_t), 0);
  if (e.err) {
    return (e_sxb_t) {err_return(ERR_FAILURE, "gc_new() failed"), NULL};
  }
  return (e_sxb_t) {NULL, e.value};
}

inline static
e_sx_t new_list(gc_global_t * g)
{
  e_void_t e = gc_new(g, &sx_vtable, sizeof(sx_t), 0);
  if (e.err) {
    return (e_sx_t) {err_return(ERR_FAILURE, "gc_new() failed"), NULL};
  }
  sx_t * x = e.value;
  x->kind = SX_LIST;
  x->lst = NULL;
  return (e_sx_t) {NULL, x};
}


inline static
e_sx_t new_atom(gc_global_t * g, sxb_t * b)
{
  sx_t * x;
  {
    e_void_t e = gc_new(g, &sx_vtable, sizeof(sx_t), 0);
    if (e.err) {
      return (e_sx_t) {err_return(ERR_FAILURE, "gc_new() failed"), NULL};
    }
    x = e.value;
  }

  if (b->at == SX_PLAIN) {
    size_t len = b->ag.sp - b->ag.s;
    char * str = b->ag.s;
    if (len && *str >= '0' && *str <= '9') {
      e_number_t e = number_setstrc(g, NULL, len, str);
      if (e.err) {
        return (e_sx_t) {err_return(ERR_FAILURE, "gc_new() failed"), NULL};
      }
      x->kind = SX_NUM;
      x->num = e.number;
      return (e_sx_t) {NULL, x};
    }
  }

  x->kind = b->at;

  {
    e_gc_str_t e = gc_new_str(g, b->ag.sp - b->ag.s, b->ag.s);
    if (e.err) {
      return (e_sx_t) {err_return(ERR_FAILURE, "gc_new_str() failed"), NULL};
    }
    x->str = e.gc_str;
  }
  return (e_sx_t) {NULL, x};
}

inline static
void ajoin(sxb_t * b, sx_t * x)
{
  sx_t * top;

  if (!b->cue) {
    if ((top = (sx_t *) gc_stack_top(b->stack))) {
      top->lst = x;
    } else {
      if (b->root) {
        b->last->next = x;
        b->last = x;
      } else {
        b->last = b->root = x;
      }
    }
  } else {
    b->cue->next = x;
  }
}

err_r * sxb_default_events(gc_global_t * g, sxb_t * b)
{
  switch (b->s) {
    case SXBS_LIST_START: {
      e_sx_t e = new_list(g);
      if (e.err) {
        return err_return(ERR_FAILURE, "new_list() failed");
      }
      ajoin(b, e.sx);
      if (gc_stack_push(g, b->stack, GC_HDR(e.sx))) {
        return err_return(ERR_FAILURE, "gc_stack_push() failed");
      }
      b->cue = NULL;
    } break;

    case SXBS_LIST_END: {
      sx_t * top = (sx_t *) gc_stack_pop(b->stack);
      assert(top && top->kind == SX_LIST);
      b->cue = top;
    } break;
    case SXBS_ATOM_START: {
    } break;
    case SXBS_ATOM_END: {
      e_sx_t e = new_atom(g, b);
      if (e.err) {
        return err_return(ERR_FAILURE, "new_atom() failed");
      }
      ajoin(b, e.sx);
      b->cue = e.sx;
#if 0
      uint32_t s = 0, u = 0;
      uint8_t mb[8], l = 0;
      //fprintf(stderr, "--- [[%.*s]]\n", (int) (b->ag.sp - b->ag.s), b->ag.s);
      fputs(">>>>>>>>>>>>>>>>>> ", stderr);
      for (char * p = b->ag.s, c; p < b->ag.sp; p++) {
        if (utf8_decode(&s, &u, c = *p) != 0) {
          if (s != 1) {
            mb[l++] = c;
            continue;
          } else {
            for (unsigned k = 0; k < l; k++) {
              fprintf(stderr, "\\%o", mb[k]);
            }
            l = 0; s = 0; c = 0;
            continue;
          }
        }
        mb[l++] = c; mb[l++] = '\0';
        fputs((char *) mb, stderr);
        l = 0; s = 0; c = 0;
      }
      fputs("\n", stderr);
#endif
    } break;
    default:
      break;
  }
  return 0;
}

uint32_t sxb_getc(sxb_t * b)
{
  if (b->bp >= b->be) {
    // fprintf(stderr, "~~~ EOB\n");
    return 0;
  }

  b->u.m &= ~UR_EOB;

  if (!(b->ba = ur_read(b->bp, b->be, &b->u)) || (b->u.m & UR_ERROR)) {
    b->v = b->s;
    b->s = SXBS_ERROR;
    b->e = SXBE_UTF8;
    return 0;
  }
  if ((b->u.m & UR_EOB)) {
    // fprintf(stderr, "--- EOB\n");
    return 0;
  }
  return b->u.c;
}

/*********************/
int u_iswhite(uint32_t c)
{
  switch (c) {
    case ' ':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
      return 1;
  }
  return 0;
}

err_r * sxb_ev(gc_global_t * g, sxb_t * b, sxb_evfn_t * evfn)
{
  uint32_t c;
  err_r  * err = NULL;

loop:
  while ((c = sxb_getc(b))) {
    if (c == '\n') {
      b->line++;
    }

#define TA(_state) do { b->s = (_state); goto trans; } while (0)
#define TC(_state) do { b->bp = b->ba; b->s = (_state); goto loop; } while (0)
#define RC(_state) \
  do {\
    ag_append(&b->ag, b->u.l, (char *) b->u.r); \
    b->bp = b->ba; \
    b->s = (_state); \
    goto loop; \
  } while (0)

#define RS(_state, _n, _s) \
  do {\
    ag_append(&b->ag, (_n), (_s)); \
    b->bp = b->ba; \
    b->s = (_state); \
    goto loop; \
  } while (0)
#define L(...) // fprintf(stderr, __VA_ARGS__);

#define TE(_msg) \
  do { \
    err = err_return(ERR_IN_INVALID, (_msg)); \
    b->s = SXBS_ERROR; \
    goto trans; \
  } while (0)

trans:
    switch ((sxbs_t) b->s) {
      case SXBS_START: {
        L("### START: ['%c']\n", c);
        TA(SXBS_INTERMEDIATE);
      }
      case SXBS_INTERMEDIATE: {
        L("### INTERMEDIATE: ['%c']\n", c);
        if (u_iswhite(c))  { TC(SXBS_INTERMEDIATE); }
        if (c == ';')      { TC(SXBS_COMMENT); }
        if (c == '(')      { TA(SXBS_LIST_START); }
        if (c == ')')      { TA(SXBS_LIST_END); }
        TA(SXBS_ATOM_START);
      }
      case SXBS_COMMENT: {
        L("### COMMENT: ['%c']\n", c);
        if (c != '\n')     { TC(SXBS_COMMENT); }
        TC(SXBS_INTERMEDIATE);
      }
      case SXBS_LIST_START: {
        L("### LIST_START: ['%c']\n", c);
        if (evfn(g, b))    { TE("evfn(LIST_START) failed"); } b->depth++;
        TC(SXBS_INTERMEDIATE);
      }
      case SXBS_LIST_END: {
        L("### LIST_END: ['%c']\n", c);
        if (b->depth == 0) { TE("unmatched parenthesis"); } b->depth--;
        if (evfn(g, b))    { TE("evfn(LIST_END) failed"); }
        TC(SXBS_INTERMEDIATE);
      }
      case SXBS_ATOM_START: {
        L("### ATOM_START: ['%c']\n", c);
        b->ag.sp = b->ag.s;
        b->aline = b->line;
        if (evfn(g, b))    { TE("evfn(ATOM_START) failed"); }
        if (c == '"' || c == '\'') {
          b->sg.ss = c;
          b->at = (c == '"') ? SX_DQ : SX_SQ;
          b->sg.s = 0;
          TC(SXBS_ESTRING);
        }
        b->at = SX_PLAIN;
        TA(SXBS_STRING);
      }
      case SXBS_STRING: {
        L("### STRING: ['%c']\n", c);
        if (!u_iswhite(c)) {
          if (c == ')')    {  TA(SXBS_ATOM_END); }
          if (c == '(')    {  TA(SXBS_ATOM_END); }
          if (c == '"')    {  TA(SXBS_ATOM_END); }
          if (c == '\'')   {  TA(SXBS_ATOM_END); }
          ag_append(&b->ag, b->u.l, (char *) b->u.r);
          TC(SXBS_STRING);
        }
        TA(SXBS_ATOM_END);
      }
      /***********************************************************/
      case SXBS_ESTRING: {
        sg_t * sg = &b->sg;
        L("### ESTRING: ['%c']\n", c);
        if (c == sg->ss)   { TC(SXBS_ATOM_END); }
        if (sg->ss == '"') {
          if (c == '\\')   { TC(SXBS_ES_ESC); }
        }
        RC(SXBS_ESTRING);
      }
      case SXBS_ES_ESC: {
        sg_t * sg = &b->sg;
        L("sss ESC: ['%c']\n", c);
        if (c == '\\')     { RC(SXBS_ESTRING); }
        if (c == sg->ss)   { RC(SXBS_ESTRING); }
        if (c == 'a')      { RS(SXBS_ESTRING, 1, "\a"); }
        if (c == 'b')      { RS(SXBS_ESTRING, 1, "\b"); }
        if (c == 'f')      { RS(SXBS_ESTRING, 1, "\f"); }
        if (c == 'n')      { RS(SXBS_ESTRING, 1, "\n"); }
        if (c == 'r')      { RS(SXBS_ESTRING, 1, "\r"); }
        if (c == 't')      { RS(SXBS_ESTRING, 1, "\t"); }
        if (c == 'v')      { RS(SXBS_ESTRING, 1, "\v"); }
        if ('0' <= c) {
          if (c <= '7')    { sg->l = 0; TA(SXBS_ES_OCT); }
        }
        if (c == 'x')      { sg->l = 0; TC(SXBS_ES_HEX); }
        if (c == 'u')      { sg->l = 0; TC(SXBS_ES_UNI); }
        ag_append(&b->ag, 1, "\\");
        RC(SXBS_ESTRING);
      }
      case SXBS_ES_OCT: {
        sg_t * sg = &b->sg;
        L("sss OCT: ['%c']\n", c);
        if ('0' <= c && c <= '7') {
          if (sg->l == 0) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_OCT);
          } else if (sg->l == 1) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_OCT);
          } else if (sg->l == 2) {
            if (sg->b[0] <= 3) {
              sg->b[sg->l++] = c - '0';
              TC(SXBS_ES_OCTW);
            }
          }
        }
        TA(SXBS_ES_OCTW);
      }
      case SXBS_ES_OCTW: {
        sg_t * sg = &b->sg;
        if (sg->l) {
          uint8_t u = 0;
          for (unsigned k = 0; k < sg->l; k++) {
            u = u << 3 | sg->b[k];
          }
          L("~~~ OCTW \\%o\n", u);
          ag_append(&b->ag, 1, (char *) &u);
        } else {
          ag_append(&b->ag, 1, "\\");
        }
        TA(SXBS_ESTRING);
      }
      case SXBS_ES_HEX: {
        sg_t * sg = &b->sg;
        if ('0' <= c && c <= '9') {
          if (sg->l == 0) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_HEX);
          } else if (sg->l == 1) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_HEXW);
          }
        }
        if ('a' <= c && c <= 'f') {
          if (sg->l == 0) {
            sg->b[sg->l++] = c - 'a' + 10;
            TC(SXBS_ES_HEX);
          } else if (sg->l == 1) {
            sg->b[sg->l++] = c - 'a' + 10;
            TC(SXBS_ES_HEXW);
          }
        }
        TA(SXBS_ES_HEXW);
      }
      case SXBS_ES_HEXW: {
        sg_t * sg = &b->sg;
        if (sg->l) {
          uint8_t u = 0;
          for (unsigned k = 0; k < sg->l; k++) {
            u = u << 4 | sg->b[k];
          }
          L("~~~ HEXW \\%x\n", u);
          ag_append(&b->ag, 1, (char *) &u);
        } else {
          ag_append(&b->ag, 2, "\\x");
        }
        TA(SXBS_ESTRING);
      }
      case SXBS_ES_UNI: {
        sg_t * sg = &b->sg;
        if ('0' <= c && c <= '9') {
          if (sg->l <= 2) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_UNI);
          } else if (sg->l == 3) {
            sg->b[sg->l++] = c - '0';
            TC(SXBS_ES_UNIW);
          }
        }
        if ('a' <= c && c <= 'f') {
          if (sg->l <= 2) {
            sg->b[sg->l++] = c - 'a' + 10;
            TC(SXBS_ES_UNI);
          } else if (sg->l == 3) {
            sg->b[sg->l++] = c - 'a' + 10;
            TC(SXBS_ES_UNIW);
          }
        }
        TA(SXBS_ES_UNIW);
      }
      case SXBS_ES_UNIW: {
        sg_t * sg = &b->sg;
        if (sg->l) {
          uint32_t u = 0;
          for (unsigned k = 0; k < sg->l; k++) {
            u = u << 4 | sg->b[k];
          }
          L("~~~ UNIW \\%x\n", u);
          uint8_t mb[6];
          ag_append(&b->ag, utf8_encode(u, mb), (char *) mb);
        } else {
          ag_append(&b->ag, 2, "\\u");
        }
        TA(SXBS_ESTRING);
      }
      /***********************************************************/
      case SXBS_ATOM_END: {
        L("### ATOM_END\n");
        if (evfn(g, b))    { TE("evfn(ATOM_END) failed"); }
        b->at = SX_NONE;
        TA(SXBS_INTERMEDIATE);
      }
      case SXBS_EOB: {
        TE("unexpected end of buffer");
      }
      case SXBS_ERROR: {
        return err;
      }
    }
  }
#undef TA
#undef TC
#undef TE
  return NULL;
}

inline static
err_r * sxb_read_len(gc_global_t * g, sxb_t * b, uint32_t len, const char str[len])
{
  b->b = str;
  b->bp = str;
  b->be = str + len;

  if(sxb_ev(g, b, sxb_default_events)) {
    return err_return(ERR_FAILURE, "sxb_ev() failed");
  }

  return NULL;
}

inline static
sx_t * sxb_sparate_root(sxb_t * b)
{
  if (b->depth == 0) {
    sx_t * r = b->root;
    b->root = NULL;
    b->cue = NULL;
    return r;
  }

  return NULL;
}

e_sx_t sxb_read(gc_global_t * g, sxb_t * b, gc_str_t * s)
{
  if (sxb_read_len(g, b, gc_str_len(s), s->data)) {
    return (e_sx_t) {err_return(ERR_PROGRESS, "sxb_read_len() failed"), NULL};
  }
  sx_t * x = sxb_sparate_root(b);
  if (!x) {
    return (e_sx_t) {err_return(ERR_PROGRESS, "buffer ended before anything was decoded"), NULL};
  }

  return (e_sx_t) {NULL, x};
}
#ifdef TEST
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/

#include <bt.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>

BT_SUITE_DEF(sx, "new sx");

BT_SUITE_SETUP_DEF(sx, objectref)
{
  gc_global_t * g = malloc(sizeof(gc_global_t));
  bt_assert_ptr_not_equal(g, NULL);

  gc_init(g);

  *objectref = g;

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(sx, objectref)
{
  gc_global_t * g = *objectref;
  gc_clear(g);
  free(g);
  return BT_RESULT_OK;
}

BT_TEST_DEF(sx, plain, object, "simple tests")
{
  gc_global_t * g = object;

  sxb_t * b;
  {
    e_sxb_t e = sxb_new(g);
    bt_chkerr(e.err);
    b = e.sxb;
  }

#define sxb_reads(_g, _b, _cstr) sxb_read((_g), (_b), gc_new_str(g, strlen(_cstr), _cstr))

  /* hello7 1world!7 */
  /*s = sxb_reads(g, b, "('\\150\\145\\154\\154\\1577\\40 1\\167\\157\\162\\154\\144\\41 7')");
  s = sxb_reads(g, b, "('\\x65\\x56f\\x9z')");
  s = sxb_reads(g, b, "('\\u257e\\u2500\\u257c' 123)");*/

  char buf[512];
  FILE * fp;
  uint32_t l;

  if (!(fp = fopen(BROOT "/src/sx/sx-tests.txt", "rb"))) {
    printf("could not open test file for reading!\n");
    return BT_RESULT_IGNORE;
  }

  while ((l = fread(buf, 1, 512, fp))) {
    for (char * p = buf, * pe = p + l; p < pe; p++) {
      bt_chkerr(sxb_read_len(g, b, 1, p));
      sx_t * s = sxb_sparate_root(b);
      if (s) {
        e_gc_str_t ee = sx_dump(g, s);
        bt_chkerr(ee.err);
        printf("%s\n", ee.gc_str->data);
      }
    }
  }

  fclose(fp);

  bt_chkerr(err_pop());

  return BT_RESULT_OK;
}
#endif /* TEST */
