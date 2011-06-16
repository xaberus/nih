#include "gc.h"
#include "tests/gc-tests.c"

#include <assert.h>

#define GC_SWEEP_MAX     40
#define GC_SWEEP_COST    10
#define GC_FINALIZE_COST 100
#define GC_STEP_SIZE     1024
#define GC_PAUSE         200
#define GC_STEP_MUL      200
#define GC_MIN_VECSZ     8
#define GC_MAX_STRHASH   (1 << 26)
#define GC_MIN_STRHASH   8

#define gc_assert(cond)      assert(cond)
#define gc_gray2black(x)     ((x)->_flags |= GC_BLACK_FLAG)
#define gc_black2gray(x)     ((x)->_flags &= (uint16_t) ~GC_BLACK_FLAG)
#define gc_is_finalized(u)   ((u)->_flags & GC_FINALIZED_FLAG)
#define gc_mark_finalized(u) ((u)->_flags |= GC_FINALIZED_FLAG)

#define gc_is_grey(x)        (!((x)->_flags & GC_COLOR_FLAGS))
#define gc_swap_white(x)     ((uint16_t) ((x)->white ^ GC_WHITE_FLAGS))
#define gc_is_dead(x, y)     ((y)->_flags & gc_swap_white(x) & GC_WHITE_FLAGS)

#define gc_curr_white(x)     ((x)->white & GC_WHITE_FLAGS)
#define gc_new_white(x, y)   ((y)->_flags = gc_curr_white(x))
#define gc_make_white(x, y) \
  ((y)->_flags = ((y)->_flags & (uint16_t) ~GC_COLOR_FLAGS) | gc_curr_white(x))
#define gc_flip_white(x)     ((x)->_flags ^= GC_WHITE_FLAGS)

#define gc_next_prepend(__list, __obj) \
  do { \
    (__obj)->_next = __list; \
    (__list) = __obj; \
  } while (0)

#define gc_list_prepend(__list, __obj) \
  do { \
    __obj->_list = __list; \
    __list = __obj; \
  } while (0)


static void gc_shrink(gc_global_t * g);
extern gc_vtable_t gc_str_vtable;


void gc_err_mem(gc_global_t * g)
{
  // TODO: ERRORS
  (void) g;
  assert(0);
}

void gc_mem_free(gc_global_t * g, size_t size, void * p)
{
  g->total -= size;
  g->alloc.realloc(g->alloc.ud, p, size, 0);
}

void * gc_mem_realloc(gc_global_t * g, size_t osz, size_t nsz, void * p)
{
  gc_assert((osz == 0) == (p == NULL));
  p = g->alloc.realloc(g->alloc.ud, p, osz, nsz);
  if (p == NULL && nsz > 0)
    gc_err_mem(g);
  gc_assert((nsz == 0) == (p == NULL));
  g->total = (g->total - osz) + nsz;
  return p;
}

void * gc_mem_grow(gc_global_t * g, size_t * szp, size_t lim, size_t esz, void * p)
{
  size_t sz = (*szp) << 1;

  if (sz < GC_MIN_VECSZ)
    sz = GC_MIN_VECSZ;
  if (sz > lim)
    sz = lim;
  p = gc_mem_realloc(g, (*szp) * esz, sz * esz, p);
  *szp = sz;
  return p;
}

void * gc_mem_new_obj(gc_global_t * g, gc_vtable_t * vtable, size_t size)
{
  gc_assert(size > sizeof(gc_obj_t));
  gc_assert(vtable);

  gc_obj_t * o = g->alloc.realloc(g->alloc.ud, NULL, 0, size);

  memset(o, 0, size);

  o->_vtable = vtable;
  o->_size = size;
  o->_next = NULL;
  o->_list = NULL;

  if (o == NULL)
    gc_err_mem(g);

  g->total += size;
  gc_next_prepend(g->root, &o->gch);
  gc_new_white(g, o);

  if (vtable->gc_init) {
    vtable->gc_init(g, o);
  }

  return o;
}

void gc_mark(gc_global_t * g, gc_hdr_t * o)
{
  gc_assert(gc_is_white(o) && !gc_is_dead(g, o));
  gc_white2gray(o);
  if (o->_vtable->gc_propagate) {
    gc_obj_t * vo = (gc_obj_t *) o; /* only objects may have propagators */
    gc_list_prepend(g->grey, vo);
  }
}


static void gc_mark_gcroot(gc_global_t * g)
{
  for (size_t i = 0; i < g->gcroot_count; i++)
    if (g->gcroot[i] != NULL)
      gc_mark_obj(g, g->gcroot[i]);
}

static void gc_mark_start(gc_global_t * g)
{
  g->grey = NULL;
  g->grey2 = NULL;
  gc_mark_gcroot(g);
  g->state = GC_STATE_PROPAGATE;
}


static size_t gc_propagate_mark(gc_global_t * g)
{
  gc_obj_t      * o = g->grey;
  gc_function_t * fn;

  gc_gray2black(o);
  g->grey = g->grey->_list;

  if ((fn = o->_vtable->gc_propagate)) {
    return fn(g, o);
  }

  gc_assert(0);
  return 0;
}

static size_t gc_propagate_grey(gc_global_t * g)
{
  size_t m = 0;

  while (g->grey != NULL)
    m += gc_propagate_mark(g);
  return m;
}

static size_t gc_separate_finalizers(gc_global_t * g, int all)
{
  size_t     m = 0;
  gc_hdr_t * h, ** p = &g->root;

  while ((h = *p) != NULL) {
    if (!(gc_is_white(h) || all) || gc_is_finalized(h)) {
      p = &h->gch._next;
    } else if (h->_vtable == &gc_str_vtable && !h->_vtable->gc_finalize) {
      p = &h->gch._next;
    } else {
      gc_obj_t * o = (gc_obj_t *) h; /* only objects may have finalizers */

      m += o->_size;
      gc_mark_finalized(o);
      *p = o->gch._next;
      if (g->fin) {
        gc_obj_t * clist = g->fin;
        o->_next = clist->_next;
        clist->_next = o;
        g->fin = o;
      } else {
        o->_next = o;
        g->fin = o;
      }
    }
  }
  return m;
}

static void gc_mark_fin(gc_global_t * g)
{
  gc_obj_t * clist = g->fin;
  gc_obj_t * u = clist;

  if (u) {
    do {
      u = u->gco._next; /* list of objects */
      gc_make_white(g, u);
      gc_mark(g, &u->gch);
    } while (u != clist);
  }
}

static gc_hdr_t ** gc_sweep(gc_global_t * g, gc_hdr_t ** p, size_t lim)
{
  uint16_t   ow = gc_swap_white(g);
  gc_hdr_t * o;

  while ((o = *p) != NULL && lim-- > 0) {
    if ((o->_flags ^ GC_WHITE_FLAGS) & ow) {
      gc_assert(!gc_is_dead(g, o) || (o->_flags & GC_FIXED_FLAG));
      gc_make_white(g, o);
      p = (gc_hdr_t **) &o->_next;
    } else {
      gc_assert(gc_is_dead(g, o) || ow == GC_SFIXED_FLAG);
      *p = o->_next;
      if (o == g->root) {
        g->root = o->gch._next;
      }

      if (o->_vtable->gc_clear) {
        o->_vtable->gc_clear(g, o);
      }
      gc_mem_free(g, o->_size, o);
    }
  }
  return p;
}

#define gc_full_sweep(x, y) gc_sweep(x, (y), SIZE_MAX)

static void gc_atomic(gc_global_t * g)
{
  size_t finsize;

  gc_mark_gcroot(g);
  gc_propagate_grey(g);

  g->grey = g->grey2;
  g->grey2 = NULL;
  gc_propagate_grey(g);

  finsize = gc_separate_finalizers(g, 0);
  gc_mark_fin(g);
  finsize += gc_propagate_grey(g);

  g->white = gc_swap_white(g);
  g->sweep = (gc_hdr_t **) &g->root;
  g->estimate = g->total - finsize;
}

static void gc_finalize(gc_global_t * g)
{
  gc_obj_t * o = g->fin->_next; /* list of objects */

  if (o == g->fin) {
    g->fin = NULL;
  } else {
    g->fin->_next = o->_next;
  }

  // o->gch._next = g->root->gch._next;
  // g->root->gch._next = o;
  o->gch._next = g->root;
  g->root = &o->gch;

  gc_make_white(g, o);

  if (o->_vtable->gc_finalize) {
    o->_vtable->gc_finalize(g, o);
  }
}

static size_t gc_one_step(gc_global_t * g)
{
  switch (g->state) {
    case GC_STATE_PAUSE: {
      gc_mark_start(g);
      return 0;
    }
    case GC_STATE_PROPAGATE: {
      if (g->grey != NULL)
        return gc_propagate_mark(g);
      g->state = GC_STATE_ATOMIC;
      return 0;
    }
    case GC_STATE_ATOMIC: {
      gc_atomic(g);
      g->state = GC_STATE_SWEEP_STRING;
      g->sweepstr = 0;
      return 0;
    }
    case GC_STATE_SWEEP_STRING: {
      size_t old = g->total;
      gc_full_sweep(g, (gc_hdr_t **) &g->strhash[g->sweepstr++]);
      if (g->sweepstr > g->strmask)
        g->state = GC_STATE_SWEEP;
      gc_assert(old >= g->total);
      g->estimate -= old - g->total;
      return GC_SWEEP_COST;
    }
    case GC_STATE_SWEEP: {
      size_t old = g->total;
      g->sweep = gc_sweep(g, g->sweep, GC_SWEEP_MAX);
      if (*g->sweep == NULL) {
        gc_shrink(g);
        if (g->fin) {
          g->state = GC_STATE_FINALIZE;
        } else {
          g->state = GC_STATE_PAUSE;
          g->debt = 0;
        }
      }
      gc_assert(old >= g->total);
      g->estimate -= old - g->total;
      return GC_SWEEP_MAX * GC_SWEEP_COST;
    }
    case GC_STATE_FINALIZE: {
      if (g->fin != NULL) {
        gc_finalize(g);
        if (g->estimate > GC_FINALIZE_COST)
          g->estimate -= GC_FINALIZE_COST;
        return GC_FINALIZE_COST;
      }
      g->state = GC_STATE_PAUSE;
      g->debt = 0;
      return 0;
    }
    default: {
      gc_assert(0);
      return 0;
    }
  }
}


void gc_full_gc(gc_global_t * g)
{
  if (g->state <= GC_STATE_ATOMIC) {
    g->sweep = (gc_hdr_t **) &g->root;
    g->grey = NULL;
    g->state = GC_STATE_SWEEP_STRING;
    g->sweepstr = 0;
  }
  while (g->state == GC_STATE_SWEEP_STRING || g->state == GC_STATE_SWEEP) {
    gc_one_step(g);
  }

  gc_assert(g->state == GC_STATE_FINALIZE || g->state == GC_STATE_PAUSE);

  g->state = GC_STATE_PAUSE;
  do {
    gc_one_step(g);
  } while (g->state != GC_STATE_PAUSE);

  g->threshold = (g->estimate / 100) * g->pause;
}

int gc_step(gc_global_t * g)
{
  size_t lim;

  lim = (GC_STEP_SIZE / 100) * g->stepmul;

  if (lim == 0)
    lim = SIZE_MAX;

  g->debt += g->total - g->threshold;

  do {
    lim -= gc_one_step(g);
    if (g->state == GC_STATE_PAUSE) {
      g->threshold = (g->estimate / 100) * g->pause;
      return 1;
    }
  } while (lim > 0);
  if (g->debt < GC_STEP_SIZE) {
    g->threshold = g->total + GC_STEP_SIZE;
  } else {
    g->debt -= GC_STEP_SIZE;
    g->threshold = g->total;
  }
  return 0;
}

void gc_add_root_obj(gc_global_t * g, gc_obj_t * o)
{
  if (g->gcroot && g->gcroot_count < g->gcroot_size) {
    g->gcroot[g->gcroot_count++] = o;
  } else {
    gc_mem_growvec(g, g->gcroot_size, SIZE_MAX, gc_obj_t *, g->gcroot);
    g->gcroot[g->gcroot_count++] = o;
  }
}

void gc_del_root_obj(gc_global_t * g, gc_obj_t * o)
{
  for (size_t k = 0; k < g->gcroot_count; k++) {
    if (g->gcroot[k] == o) {
      for (size_t n = k + 1; n < g->gcroot_count; n++) {
        g->gcroot[k++] = g->gcroot[n];
      }
      g->gcroot_count--;
      return;
    }
  }
}

void gc_free_all(gc_global_t * g)
{
  g->white = GC_WHITE_FLAGS | GC_SFIXED_FLAG;
  gc_full_sweep(g, (gc_hdr_t **) &g->root);
  size_t m = g->strmask;
  for (size_t k = 0; k <= m; k++)
    gc_full_sweep(g, (gc_hdr_t **) &g->strhash[k]);

}

void gc_barrierf(gc_global_t * g, gc_obj_t * o, gc_hdr_t * v)
{
  gc_assert(gc_is_black(o) && gc_is_white(v) && !gc_is_dead(g, v) && !gc_is_dead(g, o));
  gc_assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  if (g->state == GC_STATE_PROPAGATE || g->state == GC_STATE_ATOMIC) {
    gc_mark(g, v);
  } else {
    gc_make_white(g, o);
  }
}

void gc_barrierback(gc_global_t * g, gc_obj_t * o)
{
  gc_assert(gc_is_black(o) && !gc_is_dead(g, o));
  gc_assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  gc_black2gray(o);
  o->_list = g->grey2;
  g->grey2 = o;
}


/*************************************************************************************************/

void gc_strhash_resize(gc_global_t * g, size_t newmask)
{
  gc_str_t ** newhash;
  size_t      i;

  if (g->state == GC_STATE_SWEEP_STRING || newmask >= GC_MAX_STRHASH - 1)
    return;

  newhash = gc_mem_newvec(g, newmask + 1, gc_str_t *);
  memset(newhash, 0, (newmask + 1) * sizeof(gc_str_t *));

  if (g->strhash) {
    for (i = g->strmask; i != ~((size_t) 0); i--) {
      gc_str_t * p = g->strhash[i];
      while (p) {
        size_t     h = p->hash & newmask;
        gc_str_t * next = p->_next; /* list of strings */
        p->_next = newhash[h];
        newhash[h] = p;
        /* no barrier, since strhash is gc root */
        p = next;
      }
    }
    gc_mem_freevec(g, g->strmask + 1, gc_str_t *, g->strhash);
  }
  g->strmask = newmask;
  g->strhash = newhash;
}

/* from lookup3.c by Bob Jenkins */

#define rot(x, k) \
  (((x) << (k)) | ((x) >> (32 - (k))))

#define mix(a, b, c) \
  { \
    a -= c;  a ^= rot(c, 4);  c += b; \
    b -= a;  b ^= rot(a, 6);  a += c; \
    c -= b;  c ^= rot(b, 8);  b += a; \
    a -= c;  a ^= rot(c, 16);  c += b; \
    b -= a;  b ^= rot(a, 19);  a += c; \
    c -= b;  c ^= rot(b, 4);  b += a; \
  }

#define final(a, b, c) \
  { \
    c ^= b; c -= rot(b, 14); \
    a ^= c; a -= rot(c, 11); \
    b ^= a; b -= rot(a, 25); \
    c ^= b; c -= rot(b, 16); \
    a ^= c; a -= rot(c, 4); \
    b ^= a; b -= rot(a, 14); \
    c ^= b; c -= rot(b, 24); \
  }


uint32_t hash(const void * key, size_t length, uint32_t initval)
{
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t) length) + initval;

  {
    const uint32_t * k = (const uint32_t *) key;

    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a, b, c);
      length -= 12;
      k += 3;
    }
    if (length > 0) {
      uint32_t buff[3] = {0};
      memcpy(buff, k, length);
      a += buff[0];
      b += buff[1];
      c += buff[2];
      mix(a, b, c);
      final(a, b, c);
    }
  }

  return c;
}

size_t gc_str_clear(gc_global_t * g, void * o)
{
  (void) o;
  g->strcount--;
  return 0;
}

gc_vtable_t gc_str_vtable = {
  .gc_init = NULL,
  .gc_finalize = NULL,
  .gc_clear = gc_str_clear,
  .gc_propagate = NULL,
};



gc_str_t * gc_mem_new_str(gc_global_t * g, const char * str, size_t len)
{
  uint32_t   h = hash(str, len, 17);
  gc_str_t * s = g->strhash[h & g->strmask];

  while (s) {
    if (memcmp(str, s->data, len) == 0) {
      if (gc_is_dead(g, s)) {
        gc_flip_white(s);
      }
      return s;
    }
    s = s->_next;
  }

  size_t sz = sizeof(gc_str_t) + len + 1;
  s = gc_mem_new(g, sz);
  gc_new_white(g, s);
  s->_vtable = &gc_str_vtable;
  s->_next = NULL;
  s->_size  = sz;
  s->len = len;
  s->hash = h;
  s->id = 0;
  memcpy(s->data, str, len);
  s->data[len] = '\0';
  h &= g->strmask;
  s->_next = g->strhash[h];
  g->strhash[h] = s;
  /* no barrier, since strhash is gc root */
  if (g->strcount++ > g->strmask) {
    gc_strhash_resize(g, (g->strmask << 1) + 1);
  }
  return s;
}


/*************************************************************************************************/

static void gc_shrink(gc_global_t * g)
{
  if (g->strcount <= (g->strmask >> 2) && g->strmask > GC_MIN_STRHASH * 2 - 1) {
    gc_strhash_resize(g, g->strmask >> 1);
  }
}

gc_global_t * gc_global_init(gc_global_t * g, mem_allocator_t alloc)
{
  if (g) {
    g->white = GC_WHITE0_FLAG | GC_FIXED_FLAG;
    g->alloc = alloc;
    g->state = GC_STATE_PAUSE;

    g->strmask = 0;
    g->strcount = 0;
    g->sweepstr = 0;
    g->strhash = 0;

    g->root = NULL;
    g->sweep = (gc_hdr_t **) &g->root;
    g->grey = NULL;
    g->fin = NULL;
    g->gcroot = NULL;
    g->gcroot_count = 0;
    g->gcroot_size = 0;
    g->total = 0;
    g->threshold = 0;
    g->estimate = 0;
    g->debt = 0;
    g->pause = GC_PAUSE;
    g->stepmul = GC_STEP_MUL;

    gc_strhash_resize(g, 0xF);
    gc_mem_growvec(g, g->gcroot_size, SIZE_MAX, gc_obj_t *, g->gcroot);
  }
  return g;
}

void gc_global_clear(gc_global_t * g)
{
  gc_free_all(g);
  gc_mem_freevec(g, g->gcroot_size, gc_obj_t *, g->gcroot);
  gc_mem_freevec(g, g->strmask + 1, gc_str_t *, g->strhash);
}
