#include "gc.h"
#include "tests/gc-tests.c"

#include <assert.h>

#include <stdarg.h>

inline static
void log(unsigned N, const char * fmt, ...)
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

#define log_obj(_N, _x, _y) \
  log(_N, "# %s(g = %p [%s%s], obj = %p [%s%s%s][%u])\n", \
    __FUNCTION__, \
    (void *) _x, \
    _x->white & GC_FLAG_WHITE0 ? "w0" : "", \
    _x->white & GC_FLAG_WHITE1 ? "w1" : "", \
    (void *) _y, \
    _y->flag & GC_FLAG_WHITE0 ? "w0" : "", \
    _y->flag & GC_FLAG_WHITE1 ? "w1" : "", \
    _y->flag & GC_FLAG_BLACK ? "b" : "", \
    obj_size(_y))

void gc_mem_free(gc_global_t * g, size_t size, void * p)
{
  log(0, "# %s(%p [%zu])\n", __FUNCTION__, p, size);
  g->alloc.realloc(g->alloc.ud, p, size, 0);
}

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

#define TYPEOF(a) __typeof__(a)

#define gc_mem_new(g, s) \
  gc_mem_realloc(g, 0, (s), NULL)

#define vector_init(_g, _v, _init) \
  do { \
    TYPEOF(_v) __v = (_v); \
    gc_global_t * __g = (_g); \
    __v->count = 0; \
    __v->dsize = (_init); \
    __v->data = gc_mem_new(__g, __v->dsize * sizeof(__v->data[0])); \
    __v->psize = 0; \
    __v->proc = NULL; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
  } while (0)

#define vector_clear(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    if (__v->data) { \
      gc_mem_free(__g, __v->dsize * sizeof(__v->data[0]), __v->data); \
      __v->data = NULL; \
      __v->dsize = 0; \
      __v->count = 0; \
    } \
    if (__v->proc) { \
      gc_mem_free(__g, __v->psize * sizeof(__v->proc[0]), __v->proc); \
      __v->proc = NULL; \
      __v->psize = 0; \
    } \
  } while (0)

#define vector_reset(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    __v->count = 0; \
    memset(__v->data, 0, sizeof(TYPEOF(__v->data[0])) * __v->dsize); \
    memset(__v->proc, 0, sizeof(TYPEOF(__v->proc[0])) * __v->psize); \
  } while (0)


#define vector_process(_g, _v, _grow) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    size_t __grow = (_grow); \
    if (__v->proc) { \
      __v->proc = gc_mem_realloc(__g, __v->psize * sizeof(__v->proc[0]), __grow * sizeof(__v->proc[0]), __v->proc); \
      __v->psize = __grow; \
    } else { \
      __v->proc = gc_mem_new(__g, __grow * sizeof(__v->proc[0])); \
      __v->psize = __grow; \
    } \
    memset(__v->proc, 0, __v->psize * sizeof(__v->proc[0])); \
  } while (0)

#define vector_swap(_g, _v) \
  do { \
    TYPEOF(_v) __v = (_v); \
    if (__v->proc) { \
      TYPEOF(__v->proc) __tmp = __v->data; \
      size_t __tsize = __v->dsize; \
      __v->data = __v->proc; \
      __v->dsize = __v->psize; \
      __v->count = 0; \
      __v->psize = __tsize; \
      __v->proc = __tmp; \
    } \
  } while (0)

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

#define vector_append(_g, _v, _value) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_g) __g = (_g); \
    if (!(__v->count < __v->dsize)) { \
      size_t __sz = ALIGN16(__v->count + 1); \
      __v->data = gc_mem_realloc(__g, __v->dsize, __sz * sizeof(__v->data[0]), __v->data); \
      __v->dsize = __sz; \
    } \
    __v->data[__v->count++] =  _value; \
  } while (0)

#define vector_remove_all(_g, _v, _value) \
  do { \
    TYPEOF(_v) __v = (_v); \
    TYPEOF(_value) value = (_value); \
    for (size_t k = 0; k < __v->count; k++) { \
      if (__v->data[k] == value) { \
        for (size_t l = k + 1, m = __v->count; l < m; l++) { \
          __v->data[l - 1] = __v->data[l]; \
        } \
        __v->count--; \
      } \
    } \
  } while (0)

#define head_prepend(_l, _n) \
  do { \
    TYPEOF(_l) __l = (_l); \
    TYPEOF(_n) __n = (_n); \
    __n->next = __l->head; \
    __l->head = __n; \
  } while (0)

#define head_unlink(_l, _p, _n) \
  do { \
    TYPEOF(_l) __l = (_l); \
    TYPEOF(_p) __p = (_p); \
    TYPEOF(_n) __n = (_n); \
    *__p = __n->next; \
    if (__n == __l->head) { \
      __l->head = __n->next; \
    } \
  } while (0)

#define head_prepare(_l) \
  do { \
    TYPEOF(_l) __l = (_l); \
    __l->sweep = &__l->head; \
  } while (0)

#define head_reset(_l) \
  do { \
    TYPEOF(_l) __l = (_l); \
    __l->head = 0; \
    __l->sweep = &__l->head; \
  } while (0)

#define list_prepend(_l, _n) \
  do { \
    TYPEOF(_l) __l = (_l); \
    TYPEOF(_n) __n = (_n); \
    __n->list = __l->head; \
    __l->head = __n; \
  } while (0)

#define list_reset(_l) \
  do { \
    TYPEOF(_l) __l = (_l); \
    __l->head = 0; \
  } while (0)

#define list_pop(_l) \
  ({ \
     TYPEOF(_l) __l = (_l); \
     TYPEOF(__l->head) __o = __l->head; \
     if (__l->head) { \
       __l->head = __l->head->list; \
     } \
     __o; \
   })

#define loop_insert(_l, _n) \
  do { \
    TYPEOF(_l) __l = (_l); \
    TYPEOF(_n) __n = (_n); \
    if (__l->loop) { \
      TYPEOF(__l->loop) __clist = __l->loop; \
      __n->next = __clist->next; \
      __clist->next = __n; \
      __l->loop = __n; \
    } else { \
      __n->next = __n; \
      __l->loop = __n; \
    } \
  } while (0)

#define loop_reset(_l) \
  do { \
    TYPEOF(_l) __l = (_l); \
    __l->loop = 0; \
  } while (0)

#define loop_pop(_l) \
  ({ \
     TYPEOF(_l) __l = (_l); \
     TYPEOF(__l->loop) __r = NULL; \
     if (__l->loop) { \
       __r = __l->loop->next; \
       if (__r == __l->loop) { \
         __l->loop = 0; \
       } else { \
         __l->loop->next = __r->next; \
       } \
     } \
     __r; \
   })

static
void strings_init(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  ss->count = 0;
  ss->sweep = 0;

  ss->mask = 7;
  vector_init(g, &ss->buckets, ss->mask + 1);
  for (size_t k = 0; k < ss->buckets.dsize; k++) {
    gc_bucket_t * b = &ss->buckets.data[k];
    head_reset(b);
  }
}

static
void strings_clear(gc_global_t * g, gc_strings_t * ss)
{
  log(0, "# %s()\n", __FUNCTION__);
  ss->count = 0;
  ss->sweep = 0;
  ss->mask = 0;
  vector_clear(g, &ss->buckets);
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
    head_prepare(&b->data[k]);
  }
}

static
void strings_resize(gc_global_t * g, gc_strings_t * ss, uint32_t newmask)
{
  log(0, "# %s(%08x->%08x)\n", __FUNCTION__, ss->mask, newmask);
  gc_buckets_t * buckets = &ss->buckets;

  vector_process(g, buckets, newmask + 1);

  for (size_t i = 0; i < buckets->dsize; i++) {
    gc_bucket_t * b = &buckets->data[i];
    gc_str_t    * p = b->head, * n;
    while (p) {
      n = p->next;
      assert((p->hash & newmask) < buckets->psize);
      head_prepend(&buckets->proc[p->hash & newmask], p);
      p = n;
    }
  }
  vector_swap(g, buckets);

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
    head_reset(b);
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
  head_reset(&g->objects);
  loop_reset(&g->final);
  list_reset(&g->grey0);
  list_reset(&g->grey1);

  vector_init(g, &g->roots, 0);
}

inline static
uint32_t other_white(gc_global_t * g)
{
  return g->white ^ GC_FLAG_WHITES;
}

#define is_other(o, ow)  (((o)->flag ^ GC_FLAG_WHITES) & (ow & GC_FLAG_WHITES))
#define is_dead(o, ow)   (((o)->flag & ow) & GC_FLAG_WHITES)
#define is_fixed(o)      ((o)->flag & GC_FLAG_FIXED)
#define make_white(o, w) ((o)->flag = ((o)->flag & ~GC_FLAG_COLORS) | (w))
#define obj_size(o)      (((o)->flag & 0xffffff00) >> 8)

static
size_t sweep_objects(gc_global_t * g, gc_store_t * list, size_t limit)
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
      p = &o->next;
    } else {
      assert(is_dead(o, ow) || ow == GC_FLAG_SFIXED);

      head_unlink(list, p, o);

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
        p = &o->next;
      } else {
        assert(is_dead(o, ow) || ow == GC_FLAG_SFIXED);
        head_unlink(b, p, o);
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

  head_prepare(&g->objects);
  sweep_objects(g, &g->objects, SIZE_MAX);
  assert(!g->objects.head);

  g->white = GC_FLAG_WHITE0;
  g->state = GC_STATE_PAUSE;

  strings_reset(g, &g->strings);
  head_reset(&g->objects);
  loop_reset(&g->final);
  list_reset(&g->grey0);
  list_reset(&g->grey1);

  vector_reset(g, &g->roots);

  g->total = 0;
}

void gc_clear(gc_global_t * g)
{
  log(6, "# %s()\n", __FUNCTION__);
  gc_free_all(g);

  strings_clear(g, &g->strings);
  vector_clear(g, &g->roots);
}

#define is_white(o)   ((o)->flag & GC_FLAG_WHITES)
#define white2grey(o) ((o)->flag &= ~GC_FLAG_WHITES)
#define grey2black(o) ((o)->flag |= GC_FLAG_BLACK)

void gc_mark_obj_i(gc_global_t * g, gc_obj_t * o)
{
  log_obj(4, g, o);
  assert(is_white(o) && !is_dead(o, other_white(g)));
  white2grey(o);
  if (o->vtable->gc_propagate) {
    list_prepend(&g->grey0, o);
  } else {
    grey2black(o);
  }
}

#define str_len(s) ((((s)->flag & 0xffffff00) >> 8) - sizeof(gc_str_t) - 1)

void gc_mark_str_i(gc_global_t * g, gc_str_t * s)
{
  log_obj(4, g, s);
  assert(is_white(s) && !is_dead(s, other_white(g)));
  white2grey(s);
  grey2black(s);
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

  while ((o = list_pop(&g->grey0))) {
    log_obj(1, g, o);
    grey2black(o);
    counter += o->vtable->gc_propagate(g, o);

    if (!all)
      break;
  }

  return counter + 1;
}

#define is_final(o)  ((o)->flag & GC_FLAG_FINAL)
#define set_final(o) ((o)->flag |= GC_FLAG_FINAL)

static
size_t separate(gc_global_t * g, bool all)
{
  log(0, "# %s(%s)\n", __FUNCTION__, all ? "all" : "");
  size_t      counter = 0;
  gc_obj_t  * o;
  gc_obj_t ** p = &g->objects.head;

  while ((o = *p)) {
    if (!(is_white(o) || all) || is_final(o)) {
      p = &o->next;
    } else if (!o->vtable->gc_finalize) {
      p = &o->next;
    } else {
      set_final(o);
      head_unlink(&g->objects, p, o);
      loop_insert(&g->final, o);
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
      o = o->next;
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
  list_reset(&g->grey1);
  counter += propagate(g, 1);

  counter += separate(g, 0);
  counter += mark_final(g);
  counter += propagate(g, 1);

  g->white = other_white(g);
  head_prepare(&g->objects);

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
  gc_obj_t * o = loop_pop(&g->final);

  if (o) {
    head_prepend(&g->objects, o);
    return o->vtable->gc_finalize(g, o);
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
      list_reset(&g->grey0);
      list_reset(&g->grey1);
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
        for (gc_obj_t * o = g->objects.head; o; o = o->next) {
          log_obj(0, g, o);
        }
      }
      break;
    }
    case GC_STATE_SWEEP: {
      log(6, "# %s(%s)\n", __FUNCTION__, "SWEEP");
      counter += sweep_objects(g, &g->objects,
          g->total > 4 ? g->total >> 2 : g->total);
      if (*g->objects.sweep == 0) {
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

size_t gc_collect(gc_global_t * g, bool full)
{
  log(5, "# %s(%s)\n", __FUNCTION__, full ? "full" : "");
  size_t counter = 0;

  if (full) {
    if (g->state < GC_STATE_ATOMIC) {
      head_prepare(&g->objects);
      list_reset(&g->grey0);
      list_reset(&g->grey1);
      g->state = GC_STATE_SWEEP_STRING;
      strings_prepare(g, &g->strings); counter += 1;
    }

    while (g->state == GC_STATE_SWEEP_STRING || g->state == GC_STATE_SWEEP) {
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
  o->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  head_prepend(&g->objects, o);
  g->total++;
}

static
void intern_str(gc_global_t * g, gc_str_t * s, uint32_t size)
{
  log(0, "# %s(%p [%u])\n", __FUNCTION__, (void *) s, size);
  s->flag = (g->white & GC_FLAG_WHITES) | (size << 8);
  head_prepend(&g->strings.buckets.data[s->hash & g->strings.mask], s);
  if (g->strings.count++ > g->strings.mask) {
    uint32_t newmask = (g->strings.mask << 1) | 1;
    if (g->state != GC_STATE_SWEEP_STRING && newmask < 0xffffff - 1) {
      strings_resize(g, &g->strings, newmask);
    }
  }
}

#define flip_white(x) ((x)->flag ^= GC_FLAG_WHITES)

gc_str_t * gc_new_str(gc_global_t * g, const char * str, uint32_t len)
{
  log(0, "# %s(%.*s)\n", __FUNCTION__, len, str);
  uint32_t   h = hash(str, len, 17);

  assert((h & g->strings.mask) < g->strings.buckets.dsize);

  gc_str_t * s = g->strings.buckets.data[h & g->strings.mask].head;

  while (s) {
    if (len == str_len(s)) {
      if (memcmp(str, s->data, len) == 0) {
        if (is_dead(s, other_white(g))) {
          flip_white(s);
        }
        return s;
      }
    }
    s = s->next;
  }

  uint32_t sz = sizeof(gc_str_t) + len + 1;
  s = gc_mem_new(g, sz);
  s->next = NULL;
  s->hash  = h;
  memcpy(s->data, str, len);
  s->data[len] = '\0';

  intern_str(g, s, sz);
  return s;
}

void * gc_new_obj(gc_global_t * g, gc_vtable_t * vtable, uint32_t size)
{
  assert(size > sizeof(gc_obj_t));
  assert(vtable);

  gc_obj_t * o = gc_mem_new(g, size); memset(o, 0, size);

  memset(o, 0, sizeof(gc_obj_t));

  o->vtable = vtable;
  o->next = NULL;
  o->list = NULL;

  log(5, "# %s(<%p> [%u]) -> %p\n", __FUNCTION__, (void *) vtable, size, (void *) o);

  if (vtable->gc_init) {
    vtable->gc_init(g, o);
  }

  intern_obj(g, o, size);
  return o;
}

#define is_black(o)   ((o)->flag & GC_FLAG_BLACK)
#define black2gray(x) ((x)->flag &= ~GC_FLAG_BLACK)

void gc_barrier_back_o(gc_global_t * g, gc_obj_t * o)
{
  log_obj(1, g, o);
  assert(is_black(o) && !is_dead(o, other_white(g)));
  assert(g->state != GC_STATE_FINALIZE && g->state != GC_STATE_PAUSE);
  black2gray(o);
  list_prepend(&g->grey1, o);
}

void gc_add_root(gc_global_t * g, gc_obj_t * o)
{
  log_obj(5, g, o);
  vector_append(g, &g->roots, o);
}

void gc_del_root(gc_global_t * g, gc_obj_t * o)
{
  log_obj(5, g, o);
  vector_remove_all(g, &g->roots, o);
}
