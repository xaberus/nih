#include "sx.h"

static
size_t sx_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  sx_t * x = (sx_t *) o;
  (void) g;
  (void) argc;
  (void) ap;

  x->kind = SX_NONE;
  x->next = NULL;

  return 0;
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

static
gc_str_t * levelstr(gc_global_t * g, uint32_t level)
{
  char buf[level]; memset(buf, ' ', level);
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
  assert(a->kind & SX_ATOM);
  uint32_t l = 0;
  switch (a->kind) {
    case SX_SQ:
    case SX_DQ:
      l += 2;
    case SX_PLAIN:
      l += gc_str_len(a->str);
      break;
      return l;
    case SX_NUM:
      l += iabs(a->num->s) * 9;
      return l;
  }
  return l;
}

inline static
int isatomlist(sx_t * x)
{
  assert(x->kind & SX_LIST);
  uint32_t c = 0, l = 0;
  for (sx_t * s = x->lst; s; s = s->next) {
    if (!(s->kind & SX_ATOM)) {
      /*if ((s->kind & SX_LIST) && s->lst == NULL) {
        l += 2;
        s = s->next;
        continue;
      }*/
      return 0;
    }
    l += atom_len(s);
    c++;
  }
  return c >= 3 && l > 160;
}

uint32_t sx_writer(gc_global_t * g, gc_stack_t * st, sx_t * x, wrec_t * wr)
{
  switch (x->kind) {
    case SX_LIST: {
      uint32_t level = wr->level;
      gc_stack_push(g, st, gc_new_str(g, 1, "(")); wr->level++;
      sx_t * c = x->lst;

      if (!isatomlist(x)) {
        if (c) {
          wr->level = sx_writer(g, st, c, wr);
          c = c->next;
        }

        if (c) {
          gc_stack_push(g, st, gc_new_str(g, 1, " ")); wr->level++;
          wr->level = sx_writer(g, st, c, wr);
          c = c->next;
        }

        while (c) {
          if ((c->kind & SX_ATOM)) {
            gc_stack_push(g, st, gc_new_str(g, 1, " ")); wr->level++;
            wr->level = sx_writer(g, st, c, wr);
          } else {
            if ((c->kind & SX_LIST) && !c->lst) {
              gc_stack_push(g, st, gc_new_str(g, 1, " "));
              gc_stack_push(g, st, gc_new_str(g, 2, "()"));
              wr->level += 3;
            } else {
              gc_stack_push(g, st, gc_new_str(g, 1, "\n"));
              gc_stack_push(g, st, levelstr(g, wr->level));
              wr->level = sx_writer(g, st, c, wr);
            }
          }
          c = c->next;
        }
      } else {
        while (c) {
          gc_stack_push(g, st, gc_new_str(g, 1, "\n"));
          gc_stack_push(g, st, levelstr(g, wr->level));
          sx_writer(g, st, c, wr);
          c = c->next;
        }
      }
      gc_stack_push(g, st, gc_new_str(g, 1, ")"));
      wr->level = level;
      return level;
      break;
    }
    case SX_PLAIN:
      gc_stack_push(g, st, x->str);
      return wr->level + gc_str_len(x->str);
    case SX_SQ:
      gc_stack_push(g, st, gc_new_str(g, 1, "'"));
      gc_stack_push(g, st, x->str);
      gc_stack_push(g, st, gc_new_str(g, 1, "'"));
      return wr->level + gc_str_len(x->str) + 2;
    case SX_DQ:
      gc_stack_push(g, st, gc_new_str(g, 1, "\""));
      gc_stack_push(g, st, x->str);
      gc_stack_push(g, st, gc_new_str(g, 1, "\""));
      return wr->level + gc_str_len(x->str) + 2;
    case SX_NUM: {
      gc_stack_push(g, st, number_getdec(g, x->num));
      return wr->level + gc_str_len((gc_str_t *) gc_stack_top(st));
    }
    case SX_NONE:
    case SX_ATOM:
      fprintf(stderr, "error! (atom type %x)\n", x->kind);
      assert(0);
      break;
  }
  return 0;
}

gc_str_t * sx_dump(gc_global_t * g, sx_t * x)
{
  gc_stack_t * st = gc_new(g, &gc_stack_vtable, sizeof(gc_stack_t), 0);
  wrec_t       wr = {0};

  sx_writer(g, st, x, &wr);

  return gc_stack_strcat(g, st);
}

/**************************************************/

typedef int (sxb_evfn_t)(gc_global_t * g, sxb_t * b);

typedef __typeof__(((sxb_t *) NULL)->ag) ag_t;
typedef __typeof__(((sxb_t *) NULL)->sg) sg_t;

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

void ag_append(gc_global_t * g, ag_t * a, size_t len, const char str[len])
{
  size_t alloc = a->se - a->s;
  size_t rest = a->se - a->sp;

  if (!a->s) {
    rest = alloc = ALIGN16(len);
    a->s = a->sp = gc_mem_new(g, alloc);
    a->se = a->s + alloc;
  } else if (rest < len) {
    size_t o = a->sp - a->s;
    size_t n = alloc << 1;
    a->s = gc_mem_realloc(g, alloc, n, a->s);
    a->sp = a->s + o;
    a->se = a->s + n;
  }

  memcpy(a->sp, str, len);

  a->sp += len;
}

static
size_t sxb_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
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
  b->stack = gc_new(g, &gc_stack_vtable, sizeof(gc_stack_t), 0);

  return 0;
}

static
size_t sxb_clear(gc_global_t * g, gc_hdr_t * o)
{
  sxb_t * b = (sxb_t *) o;

  if (b->ag.s) {
    size_t alloc = b->ag.se - b->ag.s;
    gc_mem_free(g, alloc, b->ag.s);
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



sxb_t * sxb_new(gc_global_t * g)
{
  return gc_new(g, &sxb_vtable, sizeof(sxb_t), 0);
}

inline static
sx_t * new_list(gc_global_t * g)
{
  sx_t * x = gc_new(g, &sx_vtable, sizeof(sx_t), 0);
  x->kind = SX_LIST;
  x->lst = NULL;
  return x;
}


inline static
sx_t * new_atom(gc_global_t * g, sxb_t * b)
{
  sx_t * x = gc_new(g, &sx_vtable, sizeof(sx_t), 0);
  if (b->at == SX_PLAIN) {
    number_t * n = number_setstrc(g, NULL, b->ag.sp - b->ag.s, b->ag.s);
    if (n) {
      x->kind = SX_NUM;
      x->num = n;
      return x;
    }
  }

  x->kind = b->at;
  x->str = gc_new_str(g, b->ag.sp - b->ag.s, b->ag.s);
  return x;
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

int sxb_default_events(gc_global_t * g, sxb_t * b)
{
  sx_t * top, * x;

  switch (b->s) {
    case SXBS_LIST_START: {
      x = new_list(g);
      ajoin(b, x);
      gc_stack_push(g, b->stack, GC_HDR(x));
      b->cue = NULL;
    } break;

    case SXBS_LIST_END: {
      top = (sx_t *) gc_stack_pop(b->stack);
      assert(top && top->kind == SX_LIST);
      b->cue = top;
    } break;
    case SXBS_ATOM_START: {
    } break;
    case SXBS_ATOM_END: {
      x = new_atom(g, b);
      ajoin(b, x);
      b->cue = x;
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

int sxb_ev(gc_global_t * g, sxb_t * b, sxb_evfn_t * evfn)
{
  uint32_t c;

loop:
  while ((c = sxb_getc(b))) {
    if (c == '\n') {
      b->line++;
    }

#define TA(_state) do { b->s = (_state); goto trans; } while (0)
#define TC(_state) do { b->bp = b->ba; b->s = (_state); goto loop; } while (0)
#define RC(_state) \
  do {\
    ag_append(g, &b->ag, b->u.l, (char *) b->u.r); \
    b->bp = b->ba; \
    b->s = (_state); \
    goto loop; \
  } while (0)

#define RS(_state, _n, _s) \
  do {\
    ag_append(g, &b->ag, (_n), (_s)); \
    b->bp = b->ba; \
    b->s = (_state); \
    goto loop; \
  } while (0)
#define L(...) // fprintf(stderr, __VA_ARGS__);

trans:
    switch (b->s) {
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
        if (evfn(g, b))    { TA(SXBS_ERROR); } b->depth++;
        TC(SXBS_INTERMEDIATE);
      }
      case SXBS_LIST_END: {
        L("### LIST_END: ['%c']\n", c);
        if (b->depth == 0) { TA(SXBS_ERROR); } b->depth--;
        if (evfn(g, b))    { TA(SXBS_ERROR); }
        TC(SXBS_INTERMEDIATE);
      }
      case SXBS_ATOM_START: {
        L("### ATOM_START: ['%c']\n", c);
        b->ag.sp = b->ag.s;
        b->aline = b->line;
        if (evfn(g, b))    { TA(SXBS_ERROR); }
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
          ag_append(g, &b->ag, b->u.l, (char *) b->u.r);
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
        ag_append(g, &b->ag, 1, "\\");
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
          ag_append(g, &b->ag, 1, (char *) &u);
        } else {
          ag_append(g, &b->ag, 1, "\\");
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
          ag_append(g, &b->ag, 1, (char *) &u);
        } else {
          ag_append(g, &b->ag, 2, "\\x");
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
          ag_append(g, &b->ag, utf8_encode(u, mb), (char *) mb);
        } else {
          ag_append(g, &b->ag, 2, "\\u");
        }
        TA(SXBS_ESTRING);
      }
      /***********************************************************/
      case SXBS_ATOM_END: {
        L("### ATOM_END\n");
        if (evfn(g, b))    { TA(SXBS_ERROR); }
        b->at = SX_NONE;
        TA(SXBS_INTERMEDIATE);
      }
      case SXBS_ERROR: {
        L("### ERROR: ['%c']\n", c);
        assert(0);
      }
      case SXBS_EOB: {
        L("### EOB\n");
        assert(0);
        return 0;
      }
    }
  }
#undef TA
#undef TC
  return 0;
}

inline static
sx_t * sxb_read_len(gc_global_t * g, sxb_t * b, uint32_t len, const char str[len])
{
  b->b = str;
  b->bp = str;
  b->be = str + len;

  sxb_ev(g, b, sxb_default_events);
  if (b->depth == 0) {
    sx_t * r = b->root;
    b->root = NULL;
    b->cue = NULL;
    return r;
  }
  return NULL;
}

sx_t * sxb_read(gc_global_t * g, sxb_t * b, gc_str_t * s)
{
  return sxb_read_len(g, b, gc_str_len(s), s->data);
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

  mema_t a;
  a.realloc = plain_realloc;
  a.ud = NULL;

  gc_init(g, a);

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
  sx_t * s;

  sxb_t * b = sxb_new(g);

#define sxb_reads(_g, _b, _cstr) sxb_read((_g), (_b), gc_new_str(g, strlen(_cstr), _cstr))

  /* hello7 1world!7 */
  /*s = sxb_reads(g, b, "('\\150\\145\\154\\154\\1577\\40 1\\167\\157\\162\\154\\144\\41 7')");
  s = sxb_reads(g, b, "('\\x65\\x56f\\x9z')");
  s = sxb_reads(g, b, "('\\u257e\\u2500\\u257c' 123)");*/

  char buf[512];
  FILE * fp;
  uint32_t l;

  if (!(fp = fopen(BROOT "/src/sx/sx-tests.txt", "rb"))) {
    bt_log("could not open test file for reading!\n");
    return BT_RESULT_IGNORE;
  }

  while ((l = fread(buf, 1, 512, fp))) {
    for (char * p = buf, * pe = p + l; p < pe; p++) {
      s = sxb_read_len(g, b, 1, p);
      if (s) {
        gc_str_t * p = sx_dump(g, s);
        bt_log("%s\n", p->data);
      }
    }
  }

  fclose(fp);

  return BT_RESULT_OK;
}
#endif /* TEST */
