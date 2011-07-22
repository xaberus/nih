#include "gc-private.h"

#include "tests/gc-tests.c"

#include <stdarg.h>

typedef __typeof__(((gc_global_t *) NULL)->strings) gc_strings_t;
typedef __typeof__(((gc_global_t *) NULL)->strings.buckets) gc_buckets_t;
typedef __typeof__(*((gc_global_t *) NULL)->strings.buckets.data) gc_bucket_t;
typedef __typeof__(((gc_global_t *) NULL)->objects) gc_ostore_t;
typedef __typeof__(((gc_global_t *) NULL)->headers) gc_hstore_t;

#if 0
inline static
void __attribute__((format(printf, 2, 3))) log(unsigned N, const char * fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  switch (N) {
    case 0:
      // vfprintf(stderr, fmt, ap);
      break;
    case 1:
      // vfprintf(stderr, fmt, ap);
      break;
    case 2:
      // vfprintf(stderr, fmt, ap);
      break;
    case 3:
      // vfprintf(stderr, fmt, ap);
      break;
    case 4:
      // vfprintf(stderr, fmt, ap);
      break;
    case 5:
      // vfprintf(stderr, fmt, ap);
      break;
    case 6:
      // vfprintf(stderr, fmt, ap);
      break;
    default:
      break;
  }
  va_end(ap);
}
#else
  #define log(...)
#endif

#define log_obj(_N, _x, _y) \
  log(_N, "# %s(g = %p [%s%s], obj = %p [%s%s%s][%u])\n", \
    __FUNCTION__, \
    (void *) (_x), \
    (_x)->white & GC_FLAG_WHITE0 ? "w0" : "", \
    (_x)->white & GC_FLAG_WHITE1 ? "w1" : "", \
    (void *) _y, \
    GC_HDR(_y)->flag & GC_FLAG_WHITE0 ? "w0" : "", \
    GC_HDR(_y)->flag & GC_FLAG_WHITE1 ? "w1" : "", \
    GC_HDR(_y)->flag & GC_FLAG_BLACK ? "b" : "", \
    obj_size(_y))

void gc_mem_free(gc_global_t * g, size_t size, void * p)
{
  log(0, "# %s(%p [%zu])\n", __FUNCTION__, p, size);
  g->alloc.realloc(g->alloc.ud, p, size, 0);
}

gc_vtable_t gc_blob_vtable = {
  .name = "<blob>",
  .flag = GC_VT_FLAG_HDR,
  .gc_init = NULL,
  .gc_clear = NULL,
};

void * gc_mem_realloc(gc_global_t * g, size_t osz, size_t nsz, void * p)
{
  assert((osz == 0) == (p == NULL));
  assert(g);
  assert(g->alloc.realloc);
  p = g->alloc.realloc(g->alloc.ud, p, osz, nsz);
  log(0, "# %s(%p [%zu -> %zu]) -> %p", __FUNCTION__, p, osz, nsz, p);
  /*if (p == NULL && nsz > 0)
    gc_err_mem(g);*/
  assert((nsz == 0) == (p == NULL));
  assert(p || !nsz);
  return p;
}

static
void strings_init(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  ss->count = 0;
  ss->sweep = 0;

  ss->mask = 7;
  gc_vector_init(g, &ss->buckets, ss->mask + 1);
  for (size_t k = 0; k < ss->buckets.dsize; k++) {
    gc_bucket_t * b = &ss->buckets.data[k];
    gc_head_reset(b);
  }
}

static
void strings_clear(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  ss->count = 0;
  ss->sweep = 0;
  ss->mask = 0;
  gc_vector_clear(g, &ss->buckets);
}

static
void strings_prepare(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  (void) g;
  ss->sweep = 0;
  ss->run = 0;

  gc_buckets_t * b = &ss->buckets;
  for (size_t k = 0; k < b->dsize; k++) {
    gc_head_prepare(&b->data[k]);
  }
}

static
void strings_resize(gc_global_t * g, gc_strings_t * ss, uint32_t newmask)
{
  log(0, "# %s(%08x->%08x)\n", __FUNCTION__, ss->mask, newmask);
  gc_buckets_t * buckets = &ss->buckets;

  gc_vector_process(g, buckets, newmask + 1);

  for (size_t i = 0; i < buckets->dsize; i++) {
    gc_bucket_t * b = &buckets->data[i];
    gc_str_t    * p = b->head, * n;
    while (p) {
      n = (gc_str_t *) p->gch.next;
      assert((p->hash & newmask) < buckets->psize);
      gch_prepend(GC_HDRP(&buckets->proc[p->hash & newmask].head), GC_HDR(p));
      p = n;
    }
  }
  gc_vector_swap(g, buckets);

  for (uint32_t k = 0; k < buckets->dsize; k++) {
    gc_bucket_t * n = &buckets->data[k];
    n->sweep = &n->head;
  }
  ss->mask = newmask;
}

static
void strings_reset(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  (void) g;
  gc_buckets_t * buckets = &ss->buckets;

  for (size_t k = 0; k < buckets->dsize; k++) {
    gc_bucket_t * b = &buckets->data[k];
    gc_head_reset(b);
  }
}

void gc_init(gc_global_t * g, mem_allocator_t alloc)
{
  log(6, "# %s()\n", __FUNCTION__);
  g->alloc = alloc;

  g->state = GC_STATE_PAUSE;
  g->total = 0;
  g->thres = 0;
  g->white = GC_FLAG_WHITE0 | GC_FLAG_FIXED;

  strings_init(g, &g->strings);
  gc_head_reset(&g->objects);
  gc_head_reset(&g->headers);
  gc_loop_reset(&g->final);
  gc_list_reset(&g->grey0);
  gc_list_reset(&g->grey1);

  gc_vector_init(g, &g->roots, 0);
}

inline static
uint32_t other_white(gc_global_t * g)
{
  return g->white ^ GC_FLAG_WHITES;
}

#define is_other(o, ow)  ((GC_HDR(o)->flag ^ GC_FLAG_WHITES) & (ow & GC_FLAG_WHITES))
#define is_dead(o, ow)   ((GC_HDR(o)->flag & ow) & GC_FLAG_WHITES)
#define is_fixed(o)      (GC_HDR(o)->flag & GC_FLAG_FIXED)
#define make_white(o, w) (GC_HDR(o)->flag = (GC_HDR(o)->flag & ~GC_FLAG_COLORS) | (w))
#define obj_size(o)      ((GC_HDR(o)->flag & 0xffffff00) >> 8)

static
size_t sweep_objects(gc_global_t * g, gc_ostore_t * list, size_t limit)
{
  log(0, "# %s(list = %p, limit = %zu)\n", __FUNCTION__, (void *) list, limit);
  uint32_t    ow = other_white(g);
  size_t      counter = 0;
  gc_obj_t  * o;
  gc_obj_t ** p = list->sweep;

  while ((o = *p) && limit-- > 0) {
    if (is_other(o, ow)) {
      log_obj(0, g, o);
      assert(!is_dead(o, ow) || (is_fixed(o)));
      make_white(o, g->white & GC_FLAG_WHITES);
      p = (gc_obj_t **) &GC_HDR(o)->next;
    } else {
      assert(is_dead(o, ow) || ow == GC_FLAG_SFIXED);

      gch_unlink(GC_HDRP(&list->head), GC_HDRP(p), GC_HDR(o));

      log_obj(1, g, o);

      if (GC_HDR(o)->vtable->gc_clear) {
        counter += GC_HDR(o)->vtable->gc_clear(g, (gc_hdr_t *) o);
      }

      gc_mem_free(g, obj_size(o), o); counter++;

      g->total--;
    }
  }
  list->sweep = p;
  return counter + 1;
}

static
size_t sweep_headers(gc_global_t * g, gc_hstore_t * list, size_t limit)
{
  log(0, "# %s(list = %p, limit = %zu)\n", __FUNCTION__, (void *) list, limit);
  uint32_t    ow = other_white(g);
  size_t      counter = 0;
  gc_hdr_t  * o;
  gc_hdr_t ** p = list->sweep;

  while ((o = *p) && limit-- > 0) {
    if (is_other(o, ow)) {
      log_obj(0, g, o);
      assert(!is_dead(o, ow) || (is_fixed(o)));
      make_white(o, g->white & GC_FLAG_WHITES);
      p = &o->next;
    } else {
      assert(is_dead(o, ow) || ow == GC_FLAG_SFIXED);

      gch_unlink(GC_HDRP(&list->head), GC_HDRP(p), GC_HDR(o));

      log_obj(1, g, o);

      if (o->vtable->gc_clear) {
        counter += o->vtable->gc_clear(g, o);
      }

      gc_mem_free(g, obj_size(o), o); counter++;

      g->total--;
    }
  }
  list->sweep = p;
  return counter + 1;
}

static
size_t sweep_strings(gc_global_t * g, gc_strings_t * strings, size_t limit)
{
  log(0, "# %s(strings = %p, limit = %zu)\n", __FUNCTION__, (void *) strings, limit);
  uint32_t       ow = other_white(g);
  size_t         counter = 0;
  gc_str_t     * o;
  gc_str_t    ** p;
  size_t         max = (strings->mask + 1);
  size_t         run = 0;
  gc_buckets_t * buckets = &strings->buckets;

  while (run++ < max) {
    gc_bucket_t * b = &buckets->data[strings->sweep];
    p = b->sweep ? b->sweep : &b->head;
    while ((o = *p) && limit-- > 0) {
      if (is_other(o, ow)) {
        assert(!is_dead(o, ow) || (is_fixed(o)));
        make_white(o, g->white & GC_FLAG_WHITES);
        p = (gc_str_t **) &GC_HDR(o)->next;
      } else {
        assert(is_dead(o, ow) || ow == GC_FLAG_SFIXED);
        gch_unlink(GC_HDRP(&b->head), GC_HDRP(p), GC_HDR(o));
        gc_mem_free(g, obj_size(o), o); counter++; strings->count--;
      }
      limit--;
    }
    b->sweep = p;
    strings->sweep = (strings->sweep + 1) & strings->mask;
  }

  strings->sweep = (strings->sweep + 1) & strings->mask;
  strings->run += run;
  return counter + 1;
}

static
size_t step(gc_global_t * g);

static
void gc_free_all(gc_global_t * g)
{
  g->white = GC_FLAG_WHITES | GC_FLAG_SFIXED;

  strings_prepare(g, &g->strings);
  sweep_strings(g, &g->strings, SIZE_MAX);

  gc_head_prepare(&g->headers);
  sweep_headers(g, &g->headers, SIZE_MAX);
  assert(!g->headers.head);

  gc_head_prepare(&g->objects);
  sweep_objects(g, &g->objects, SIZE_MAX);
  assert(!g->objects.head);

  g->white = GC_FLAG_WHITE0;
  g->state = GC_STATE_PAUSE;

  strings_reset(g, &g->strings);
  gc_head_reset(&g->objects);
  gc_head_reset(&g->headers);
  gc_loop_reset(&g->final);
  gc_list_reset(&g->grey0);
  gc_list_reset(&g->grey1);

  gc_vector_reset(g, &g->roots);

  g->total = 0;
}

void gc_clear(gc_global_t * g)
{
  log(6, "# %s()\n", __FUNCTION__);
  gc_free_all(g);

  strings_clear(g, &g->strings);
  gc_vector_clear(g, &g->roots);
}

#define is_white(o)   (GC_HDR(o)->flag & GC_FLAG_WHITES)
#define white2grey(o) (GC_HDR(o)->flag &= ~GC_FLAG_WHITES)
#define grey2black(o) (GC_HDR(o)->flag |= GC_FLAG_BLACK)

void gc_mark_obj_i(gc_global_t * g, gc_obj_t * o)
{
  log_obj(4, g, o);
  assert(is_white(o) && !is_dead(o, other_white(g)));
  white2grey(o);
  if (GC_HDR(o)->vtable->gc_propagate) {
    gcl_prepend(&g->grey0.head, o);
  } else {
    grey2black(o);
  }
}

void gc_mark_hdr_i(gc_global_t * g, gc_hdr_t * o)
{
  log_obj(4, g, o);
  assert(is_white(o) && !is_dead(o, other_white(g)));
  white2grey(o);
  grey2black(o);
}

static
size_t mark_roots(gc_global_t * g)
{
  log(4, "# %s()\n", __FUNCTION__);
  size_t counter = 0;
  for (size_t k = 0; k < g->roots.count; k++) {
    gc_mark_obj(g, g->roots.data[k]);
    counter++;
  }

  return counter + 1;
}

static
size_t propagate(gc_global_t * g, bool all)
{
  size_t     counter = 0;
  gc_obj_t * o;

  while ((o = gcl_pop(&g->grey0.head))) {
    log_obj(1, g, o);
    grey2black(o);
    counter += GC_HDR(o)->vtable->gc_propagate(g, o);

    if (!all)
      break;
  }

  return counter + 1;
}

#define is_final(o)  (GC_HDR(o)->flag & GC_FLAG_FINAL)
#define set_final(o) (GC_HDR(o)->flag |= GC_FLAG_FINAL)

static
size_t separate(gc_global_t * g, bool all)
{
  log(0, "# %s(%s)\n", __FUNCTION__, all ? "all" : "");
  size_t      counter = 0;
  gc_obj_t  * o;
  gc_obj_t ** p = &g->objects.head;

  while ((o = *p)) {
    if (!(is_white(o) || all) || is_final(o)) {
      p = (gc_obj_t **) &GC_HDR(o)->next;
    } else if (!GC_HDR(o)->vtable->gc_finalize) {
      p = (gc_obj_t **) &GC_HDR(o)->next;
    } else {
      set_final(o);
      gch_unlink(GC_HDRP(&g->objects.head), GC_HDRP(p), GC_HDR(o));
      gclp_insert(&g->final.loop, o);
      counter += 2 + 1;
    }
  }

  return counter + 1;
}

static
size_t mark_final(gc_global_t * g)
{
  log(0, "# %s()\n", __FUNCTION__);
  size_t     counter = 0;
  gc_obj_t * clist = g->final.loop;
  gc_obj_t * o = clist;

  if (o) {
    do {
      o = (gc_obj_t *) GC_HDR(o)->next;
      make_white(o, g->white);
      gc_mark_obj(g, o);
      counter += 2;
    } while (o != clist);
  }

  return counter + 1;
}

static
size_t atomic(gc_global_t * g)
{
  log(0, "# %s()\n", __FUNCTION__);
  size_t counter = 0;
  counter += mark_roots(g);
  counter += propagate(g, 1);

  g->grey0.head = g->grey1.head;
  gc_list_reset(&g->grey1);
  counter += propagate(g, 1);

  counter += separate(g, 0);
  counter += mark_final(g);
  counter += propagate(g, 1);

  g->white = other_white(g);
  gc_head_prepare(&g->objects);
  gc_head_prepare(&g->headers);

  return counter + 1;
}

static
void shrink(gc_global_t * g)
{
  log(0, "# %s()\n", __FUNCTION__);
  if (g->strings.count <= (g->strings.mask >> 2) && g->strings.mask > 7 * 2 - 1) {
    uint32_t newmask = g->strings.mask >> 1;
    if (g->state != GC_STATE_SWEEP_STRING || newmask < 0xffffff - 1)
      strings_resize(g, &g->strings, newmask);
  }
}

static
size_t finalize(gc_global_t * g)
{
  log(0, "# %s()\n", __FUNCTION__);
  gc_obj_t * o = gclp_pop(&g->final.loop);

  if (o) {
    gch_prepend(GC_HDRP(&g->objects.head), GC_HDR(o));
    return GC_HDR(o)->vtable->gc_finalize(g, o);
  }

  return 0;
}

static
size_t step(gc_global_t * g)
{
  size_t counter = 0;

  switch (g->state) {
    case GC_STATE_PAUSE: {
      log(6, "# %s(%s)\n", __FUNCTION__, "PAUSE");
      gc_list_reset(&g->grey0);
      gc_list_reset(&g->grey1);
      mark_roots(g);
      strings_prepare(g, &g->strings);
      g->state = GC_STATE_PROPAGATE;
      break;
    }
    case GC_STATE_PROPAGATE: {
      log(6, "# %s(%s)\n", __FUNCTION__, "PROPAGATE");
      if (g->grey0.head) {
        counter += propagate(g, 0);
      } else {
        g->state = GC_STATE_ATOMIC;
      }
      break;
    }
    case GC_STATE_ATOMIC: {
      log(6, "# %s(%s)\n", __FUNCTION__, "ATOMIC");
      counter += atomic(g);
      g->state = GC_STATE_SWEEP_STRING;
      /* strings_prepare(g, &g->strings); */
      break;
    }
    case GC_STATE_SWEEP_STRING: {
      log(6, "# %s(%s)\n", __FUNCTION__, "SWEEP_STRING");
      counter += sweep_strings(g, &g->strings,
          g->strings.count > 100 ? g->strings.count >> 2 : 100);
      if (g->strings.run > g->strings.mask) {
        g->state = GC_STATE_SWEEP;
      }
      break;
    }
    case GC_STATE_SWEEP: {
      log(6, "# %s(%s)\n", __FUNCTION__, "SWEEP");
      counter += sweep_objects(g, &g->objects,
          g->total > 4 ? g->total >> 2 : g->total);
      if (*g->objects.sweep == 0) {
        g->state = GC_STATE_SWEEP_HEADER;
      }
      break;
    }
    case GC_STATE_SWEEP_HEADER: {
      log(6, "# %s(%s)\n", __FUNCTION__, "SWEEP_HEADER");
      counter += sweep_headers(g, &g->headers,
          g->total > 4 ? g->total >> 2 : g->total);
      if (*g->headers.sweep == 0) {
        shrink(g);
        if (g->final.loop) {
          g->state = GC_STATE_FINALIZE;
        } else {
          g->state = GC_STATE_PAUSE;
        }
      }
      break;
    }
    case GC_STATE_FINALIZE: {
      log(5, "# %s(%s)\n", __FUNCTION__, "FINALIZE");
      if (g->final.loop) {
        counter += finalize(g);
        break;
      }
      g->state = GC_STATE_PAUSE;
      break;
    }
  }
  return counter + 1;
}

#define sweeping(_g) \
  ((_g)->state == GC_STATE_SWEEP_STRING \
    || (_g)->state == GC_STATE_SWEEP \
    || (_g)->state == GC_STATE_SWEEP_HEADER)

size_t gc_collect(gc_global_t * g, bool full)
{
  log(5, "# %s(%s)\n", __FUNCTION__, full ? "full" : "");
  size_t counter = 0;

  if (full) {
    if (g->state < GC_STATE_ATOMIC) {
      gc_head_prepare(&g->objects);
      gc_head_prepare(&g->headers);
      gc_list_reset(&g->grey0);
      gc_list_reset(&g->grey1);
      g->state = GC_STATE_SWEEP_STRING;
      strings_prepare(g, &g->strings); counter += 1;
    }

    while (sweeping(g)) {
      counter += step(g);
    }

    assert(g->state == GC_STATE_FINALIZE || g->state == GC_STATE_PAUSE);

    g->state = GC_STATE_PAUSE;
    do {
      counter += step(g);
    } while (g->state != GC_STATE_PAUSE);

    // TODO threshold
    return counter + 1;
  }

  // TODO: metric
  ssize_t limit = 20;

  do {
    size_t c = step(g); counter += c; limit -= c;
    if (g->state == GC_STATE_PAUSE) {
      return counter + 1;
    }
  } while (limit > 0);

  return counter + 1;
}

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

static
void intern_obj(gc_global_t * g, gc_obj_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->objects.head), GC_HDR(o));
  g->total++;
}

static
void intern_hdr(gc_global_t * g, gc_hdr_t * o, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) o, size);
  GC_HDR(o)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->headers.head), GC_HDR(o));
  g->total++;
}

static
void intern_str(gc_global_t * g, gc_str_t * s, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) s, size);
  GC_HDR(s)->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  gch_prepend(GC_HDRP(&g->strings.buckets.data[s->hash & g->strings.mask].head), GC_HDR(s));
  if (g->strings.count++ > g->strings.mask) {
    uint32_t newmask = (g->strings.mask << 1) | 1;
    if (g->state != GC_STATE_SWEEP_STRING && newmask < 0xffffff - 1) {
      strings_resize(g, &g->strings, newmask);
    }
  }
}

#define flip_white(x) (GC_HDR(x)->flag ^= GC_FLAG_WHITES)

static
gc_vtable_t gc_str_vtable = {
  .name = "testobj_t",
  .flag = GC_VT_FLAG_STR,
};

gc_str_t * gc_new_str(gc_global_t * g, uint32_t len, const char str[len])
{
  log(0, "# %s(%.*s)\n", __FUNCTION__, len, str);
  uint32_t   h = hash(str, len, 17);

  assert((h & g->strings.mask) < g->strings.buckets.dsize);

  gc_str_t * s = g->strings.buckets.data[h & g->strings.mask].head;

  while (s) {
    if (len == gc_str_len(s)) {
      if (memcmp(str, s->data, len) == 0) {
        if (is_dead(s, other_white(g))) {
          flip_white(s);
        }
        return s;
      }
    }
    s = (gc_str_t *) GC_HDR(s)->next;
  }

  uint32_t sz = sizeof(gc_str_t) + len + 1;
  s = gc_mem_new(g, sz);
  GC_HDR(s)->vtable = &gc_str_vtable;
  GC_HDR(s)->next = NULL;
  s->hash = h;
  memcpy(s->data, str, len);
  s->data[len] = '\0';

  intern_str(g, s, sz);
  return s;
}

gc_str_t * gc_new_strf(gc_global_t * g, const char * fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  uint32_t len = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  char tmp[len + 1];
  va_start(ap, fmt);
  vsnprintf(tmp, len + 1, fmt, ap);
  va_end(ap);
  return gc_new_str(g, strlen(tmp), tmp);
}

void * gc_new(gc_global_t * g, gc_vtable_t * vtable, uint32_t size)
{
  assert(vtable);
  assert(vtable->flag);
  assert(size > sizeof(gc_hdr_t));

  gc_hdr_t * o = gc_mem_new(g, size);

  GC_HDR(o)->vtable = vtable;

  log(5, "# %s(<%p> [%u]) -> %p\n", __FUNCTION__, (void *) vtable, size, (void *) o);

  if (vtable->gc_init) {
    vtable->gc_init(g, o);
  }

  if ((vtable->flag & GC_VT_FLAG_OBJ) == GC_VT_FLAG_OBJ) {
    intern_obj(g, (gc_obj_t *) o, size);
  } else if ((vtable->flag & GC_VT_FLAG_HDR) == GC_VT_FLAG_HDR) {
    intern_hdr(g, o, size);
  } else {
    if (vtable->gc_clear) {
      vtable->gc_clear(g, o);
    }
    gc_mem_free(g, size, o);
    o = NULL;
    assert(0);
  }
  return o;
}

#define is_black(_o)   (GC_HDR(_o)->flag & GC_FLAG_BLACK)
#define black2gray(_o) (GC_HDR(_o)->flag &= ~GC_FLAG_BLACK)

void gc_barrier_oo(gc_global_t * g, gc_obj_t * o, gc_obj_t * v)
{
  assert(is_black(o) && is_white(v) && !is_dead(v, other_white(g)) && !is_dead(o, other_white(g)));
  assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  if (g->state == GC_STATE_PROPAGATE || g->state == GC_STATE_ATOMIC) {
    gc_mark_obj(g, v);
  } else {
    make_white(o, g->white);
  }
}

void gc_barrier_oh(gc_global_t * g, gc_obj_t * o, gc_hdr_t * h)
{
  assert(is_black(o) && is_white(h) && !is_dead(h, other_white(g)) && !is_dead(o, other_white(g)));
  assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  if (g->state == GC_STATE_PROPAGATE || g->state == GC_STATE_ATOMIC) {
    gc_mark(g, h);
  } else {
    make_white(o, g->white);
  }
}


void gc_barrier_back_o(gc_global_t * g, gc_obj_t * o)
{
  log_obj(1, g, o);
  assert(is_black(o) && !is_dead(o, other_white(g)));
  assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  black2gray(o);
  gcl_prepend(&g->grey1.head, o);
}

void gc_add_root(gc_global_t * g, gc_obj_t * o)
{
  log_obj(5, g, o);
  gc_vector_append(g, &g->roots, o);
}

void gc_del_root(gc_global_t * g, gc_obj_t * o)
{
  log_obj(5, g, o);
  gc_vector_remove_all(g, &g->roots, o);
}
