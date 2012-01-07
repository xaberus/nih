#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>

#include "store.h"

#include "store-tests.c"

#define VERBOSEDEBUG 1

uint32_t hash32(uint32_t M)
{
  M = (M + 0x7ed55d16) + (M << 12);
  M = (M ^ 0xc761c23c) ^ (M >> 19);
  M = (M + 0x165667b1) + (M << 5);
  M = (M + 0xd3a2646c) ^ (M << 9);
  M = (M + 0xfd7046c5) + (M << 3);
  M = (M ^ 0xb55a4f09) ^ (M >> 16);
  return M;
}

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

spman_t * spman_init(gc_global_t * g, spman_t * pm, int fd, off_t offset, uint32_t cnt)
{
  if (!pm || fd == -1)
    return NULL;

  (void) g;

  pm->fd = fd;
  pm->offset = offset;
  pm->cnt = cnt;
  pm->anum = 0;
  pm->maps = 0;
  memset(pm->map, 0, sizeof(spmap_t *) * STORE_LIVEPAGES);

  return pm;
}

int spman_clear(gc_global_t * g, spman_t * pm)
{
  int fd = pm->fd;

  for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
    spmap_t * m = pm->map[k], * n;
    while (m) {
      n = m->next;
      spman_unref(g, pm, m);
      m = n;
    }
  }

  memset(pm->map, 0, sizeof(spmap_t *) * STORE_LIVEPAGES);
  pm->maps = 0;
  pm->fd = -1;
  pm->offset = -1;
  pm->cnt = 0;

  return fd;
}

spmap_t * spman_load(gc_global_t * g, spman_t * pm, uint32_t pnum)
{
  if (pnum >= pm->cnt)
    return NULL;

  {
    uint32_t  hash = hash32(pnum) & STORE_LIVEMASK;
    spmap_t * c = pm->map[hash];
    while (c) {
      if (c->pnum == pnum) {
        return c;
      }
      c = c->next;
    }
  }

  if (pm->maps >= STORE_LIVEPAGES) {
    uint32_t minc = 0, min = 0;
    for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
      spmap_t * n = pm->map[k];
      while (n) {
        if (min) {
          minc = minc > n->inuse ? n->inuse : minc;
        } else {
          min = 1;
          minc = n->inuse;
        }
        n = n->next;
      }
    }
    spmap_t * del = NULL;
    for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
      spmap_t * n = pm->map[k];
      while (n) {
        if (n->inuse == minc) {
          del = n;
          break;
        }
        n = n->next;
      }
      if (del) {
        break;
      }
    }
    if (del) {
      spman_unref(g, pm, del);
    }
  }

  size_t off = pm->offset + sizeof(spage_t) * pnum;
  spage_t * b = mmap(NULL, sizeof(spage_t), PROT_READ | PROT_WRITE, MAP_SHARED, pm->fd, off);
  if (b == MAP_FAILED)
    return NULL;

  spmap_t * m = gc_mem_new(g, sizeof(spmap_t));
  m->pnum = pnum;
  m->inuse = 0;
  m->ref = 0;
  m->page = b;

  uint32_t hash = hash32(pnum) & STORE_LIVEMASK;

  if (!pm->map[hash]) {
    pm->map[hash] = m;
    m->next = NULL;
    m->rev = &pm->map[hash];
  } else {
    m->next = pm->map[hash];
    m->next->rev = &m->next;
    pm->map[hash] = m;
    m->rev = &pm->map[hash];
  }

  pm->maps++;

#if VERBOSEDEBUG
  fprintf(stderr, "[P] loaded page [1;33m%p[0;m:[1;32m%u[0;m\n", (void *) m->page, pnum);
#endif

  return spman_ref(g, pm, m);;
}

void spman_unload(gc_global_t * g, spman_t * pm, spmap_t * m)
{
#if VERBOSEDEBUG
    fprintf(stderr, "{P} unloading page [1;32m%u[0;m\n", m->pnum);
#endif
    *(m->rev) = m->next;
    if (m->next) {
      m->next->rev = m->rev;
    }
    munmap(m->page, sizeof(spage_t));
    gc_mem_free(g, sizeof(spmap_t), m);
    pm->maps--;
}

spmap_t * spman_ref(gc_global_t * g, spman_t * pm, spmap_t * m)
{
  (void) g;
  (void) pm;
  m->ref++;
  return m;
}
void spman_unref(gc_global_t * g, spman_t * pm, spmap_t * m)
{
  if (m->ref > 1) {
    m->ref--;
  } else {
    spman_unload(g, pm, m);
  }
}

sdrec_t spmap_alloc(gc_global_t * g, spmap_t * m, uint16_t size, uint16_t usage)
{
  uint32_t  sz = ALIGN16(size);
  uint16_t  offset = 0;
  spage_t * p = m->page;
  sslot_t * t = &p->info;
  for (uint16_t k = 0; k < STORE_PAGESIZE; k++) {
    if (t->flag == 0 && (offset + sz) <= STORE_DATASIZE) {
      t->size = sz;
      t->offset = offset;
      t->flag = SSLOT_FLAG_DATA | (usage & SSLOT_USAGEMASK);
      return (sdrec_t) {
        .id = (m->pnum << STORE_SLOTBITS) | k,
        .size = t->size,
        .flag = t->flag,
        .slot = &p->data[t->offset],
        .map = m,
      };
    }
    offset = t->offset + t->size;
    t++;
  }
  return (sdrec_t) {
    .id = SRID_NIL,
    .size = 0,
    .flag = 0,
    .slot = NULL,
    .map = NULL,
  };
}


sdrec_t spman_add(gc_global_t * g, spman_t * pm, uint16_t size, uint16_t usage)
{
  spmap_t * m;

  while ((m = spman_load(g, pm, pm->anum))) {
    sdrec_t r = spmap_alloc(g, m, size, usage);
    if (r.id != SRID_NIL) {
      return r;
    }
    pm->anum++;
  }

  /* no free slots found till now */

  if (!ftruncate(pm->fd, pm->offset + (pm->cnt + 1) * sizeof(spage_t))) {
    pm->cnt++;
#if VERBOSEDEBUG
    fprintf(stderr, "[P] resizing store to %u pages (anum is %u)\n", pm->cnt, pm->anum);
#endif
    if ((m = spman_load(g, pm, pm->anum))) {
      return spmap_alloc(g, m, size, usage);
    }
  }

  return (sdrec_t) {
    .id = SRID_NIL,
    .size = 0,
    .flag = 0,
    .slot = NULL,
    .map = NULL,
  };
}

#define SRID_TO_PAGE(_srid) (((_srid) & STORE_PAGEMASK) >> STORE_SLOTBITS)
#define SRID_TO_SLOT(_srid) ((_srid) & STORE_SLOTMASK)

sdrec_t spman_get(gc_global_t * g, spman_t * pm, srid_t id)
{
  uint32_t  pnum = SRID_TO_PAGE(id);
  uint16_t  snum = SRID_TO_SLOT(id);
  uint32_t  hash = hash32(pnum) & STORE_LIVEMASK;
  spmap_t * m = pm->map[hash];

  while (m) {
    if (m->pnum == pnum) {
      break;
    }
    m = m->next;
  }

  if (!m) {
    m = spman_load(g, pm, pnum);
  }

  if (m) {
    spage_t * p = m->page;
    if (p->info[snum].flag & SSLOT_FLAG_DATA) {
      return (sdrec_t) {
        .id = id,
        .size = p->info[snum].size,
        .flag = p->info[snum].flag,
        .slot = &p->data[p->info[snum].offset],
        .map = m,
      };
    }
  }

  return (sdrec_t) {
    .id = SRID_NIL,
    .size = 0,
    .flag = 0,
    .slot = NULL,
    .map = NULL,
    };
}

/*************************************************************************************************/

inline static
err_t srid_insert(trie_t * t, srid_t id, void * p, bool rep)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  uint64_t value = (uint64_t) p;
  return trie_insert(t, sizeof(key.b), key.b, value, rep);
}

inline static
err_t srid_delete(trie_t * t, srid_t id)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  return trie_delete(t, sizeof(key.b), key.b);
}

inline static
err_t srid_find(trie_t * t, srid_t id, void ** p)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  uint64_t value;
  err_t err = trie_find(t, sizeof(key.b), key.b, &value);
  *p = (void *) value;
  return err;
}

/*************************************************************************************************/

static
size_t sclass_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  store_t  * s = (store_t *) g;
  sclass_t * c = (sclass_t *) o;

  if (argc != 3) {
    gc_error(g, "sclass: init without arguments");
  }

  c->id = va_arg(ap, srid_t);
  c->cnt = va_arg(ap, int);

  uint8_t * p = va_arg(ap, uint8_t *);

  for (uint16_t k = 0; k < c->cnt; k++) {
    c->kind[k] = *((srid_t *) p);
    p += sizeof(srid_t);
  }

  err_t err = srid_insert(&s->i2r, c->id, c, 0);
  if (err) {
    gc_error(g, "sclass: attempted to replace id %u", (unsigned) c->id);
  }

  spmap_t * m = spman_load(g, &s->pm, SRID_TO_PAGE(c->id));
  if (!m) {
    gc_error(g, "sclass: could not load page %u", (unsigned) SRID_TO_PAGE(c->id));
  }
  m->inuse++;

  return 0;
}

static
size_t sclass_clear(gc_global_t * g, gc_hdr_t * o)
{
  store_t  * s = (store_t *) g;
  sclass_t * c = (sclass_t *) o;

  err_t err = srid_delete(&s->i2r, c->id);
  if (err) {
    gc_error(g, "smrec: attempted to delete non-existent id %u", (unsigned) c->id);
  }

  spmap_t * m = spman_load(g, &s->pm, SRID_TO_PAGE(c->id));
  if (!m) {
    gc_error(g, "sclass: could not load page %u", (unsigned) SRID_TO_PAGE(c->id));
  }
  m->inuse--;

  return 0;
}

gc_vtable_t sclass_vtable = {
  .flag = GC_VT_FLAG_HDR,
  .gc_init = sclass_init,
  .gc_clear = sclass_clear,
};

sclass_t * store_get_class(store_t * s, srid_t id)
{
  sdrec_t r = spman_get(&s->g, &s->pm, id);
  if (r.id == SRID_NIL) {
    return NULL;
  }
  void * c = NULL;
  if ((r.flag & SSLOT_USAGEMASK) == SSLOT_USAGE_CLASS) {
    if (srid_find(&s->i2r, r.id, &c)) {
      uint16_t cnt = *((uint16_t *) r.slot);
      spman_ref(&s->g, &s->pm, r.map);
      c = gc_new(&s->g, &sclass_vtable, sizeof(sclass_t) + sizeof(skind_t) * cnt,
            3, id, cnt, r.slot + sizeof(uint16_t));
      spman_unref(&s->g, &s->pm, r.map);
    }
  }
  return c;
}

uint16_t sclass_mem_size(sclass_t * c)
{
  uint16_t sz = 0;
  for (uint16_t k = 0; k < c->cnt; k++) {
    switch (c->kind[k]) {
      case SKIND_INT32:
      case SKIND_UINT32:
        sz += sizeof(uint32_t); break;
      case SKIND_INT64:
      case SKIND_UINT64:
        sz += sizeof(uint64_t); break;
      case SKIND_DOUBLE:
        sz += sizeof(double); break;
      case SKIND_STRING:
        sz += sizeof(gc_str_t *); break;
      case SKIND_OBJECT:
        sz += sizeof(smrec_t *); break;
      case SKIND_CLASS:
        sz += sizeof(sclass_t *); break;
    }
  }
  return sz;
}

size_t sclass_walk_propagate(store_t * s, sclass_t * c, void * p)
{
  size_t k = 0;
  for (uint16_t k = 0; k < c->cnt; k++) {
    switch (c->kind[k]) {
      case SKIND_INT32:
      case SKIND_UINT32:
        p = (uint8_t *) p + sizeof(uint32_t); break;
      case SKIND_INT64:
      case SKIND_UINT64:
        p = (uint8_t *) p + sizeof(uint64_t); break;
      case SKIND_DOUBLE:
        p = (uint8_t *) p + sizeof(double); break;
      case SKIND_STRING:
        gc_mark(&s->g, *((gc_hdr_t **) p)); k++;
        p = (uint8_t *) p + sizeof(gc_str_t *);
        break;
      case SKIND_OBJECT:
        gc_mark(&s->g, *((gc_hdr_t **) p)); k++;
        p = (uint8_t *) p + sizeof(smrec_t *);
        break;
      case SKIND_CLASS:
        gc_mark(&s->g, *((gc_hdr_t **) p)); k++;
        p = (uint8_t *) p + sizeof(sclass_t *);
        break;
    }
  }
  return k;
}

/*************************************************************************************************/

static
size_t smrec_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  store_t * s = (store_t *) g;
  smrec_t * r = (smrec_t *) o;

  if (argc != 3) {
    gc_error(g, "smrec: init without arguments");
  }

  r->sc = va_arg(ap, sclass_t *);
  r->id = va_arg(ap, srid_t);

  r->sz = sclass_mem_size(r->sc);

  uint8_t * slot = va_arg(ap, uint8_t *);

  r->ptr = gc_mem_new(g, r->sz);

#if VERBOSEDEBUG
  fprintf(stderr, "{M} object <[1;33m%p[0;m:[1;31m%u[0;m> of class <[1;33m%p[0;m:[1;35m%u[0;m>\n",
    (void *) r, r->id, (void *) r->sc, r->sc->id);
#endif

  uint8_t * sp = slot;
  uint8_t * dp = r->ptr;
  for (uint16_t k = 0; k < r->sc->cnt; k++) {
    switch (r->sc->kind[k]) {
      case SKIND_INT32:
        *((int32_t *) dp) = *((int32_t *) sp);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (int32) <%d>\n",
          (void *) r, r->id, *((int32_t *) dp));
#endif
        sp += sizeof(int32_t);
        dp += sizeof(int32_t);
        break;
      case SKIND_UINT32:
        *((uint32_t *) dp) = *((uint32_t *) sp);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (uint32) <%u>\n",
          (void *) r, r->id, *((int32_t *) dp));
#endif
        sp += sizeof(uint32_t);
        dp += sizeof(uint32_t);
        break;
      case SKIND_INT64:
        *((int64_t *) dp) = *((int64_t *) sp);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (int64) <%ld>\n",
          (void *) r, r->id, (long) *((int64_t *) dp));
#endif
        sp += sizeof(int64_t);
        dp += sizeof(int64_t);
        break;
      case SKIND_UINT64:
        *((uint64_t *) dp) = *((uint64_t *) sp);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (uint64) <%lu>\n",
          (void *) r, r->id, (long unsigned) *((uint64_t *) dp));
#endif
        sp += sizeof(uint64_t);
        dp += sizeof(uint64_t);
        break;
      case SKIND_DOUBLE:
        *((double *) dp) = *((double *) sp);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (double) <%g>\n",
          (void *) r, r->id, *((double *) dp));
#endif
        sp += sizeof(double);
        dp += sizeof(double);
        break;
      case SKIND_STRING: {
        uint16_t l = *((uint16_t *) sp);
        sp += sizeof(uint16_t);
        gc_str_t * str = gc_new_str(&s->g, l, (char *) sp);
        *((gc_str_t **) dp) = str;
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (string) <%.*s>\n",
          (void *) r, r->id, gc_str_len(str), str->data);
#endif
        sp += ALIGN16(l);
        dp += sizeof(gc_str_t *);
        break;
      }
      case SKIND_OBJECT: {
        *((smrec_t **) dp) = store_get_object(s, *((srid_t *) sp));
#if VERBOSEDEBUG
        if (*((void **) dp)) {
          fprintf(stderr, "    element of <%p:%u> (object) <[1;33m%p[0;m:[1;31m%u[0;m:[1;35m%u[0;m>\n",
            (void *) r, r->id, *((void **) dp), (*((smrec_t **) dp))->id, (*((smrec_t **) dp))->sc->id);
        } else {
          fprintf(stderr, "    element of <%p:%u> (object) <[1;33m%p[0;m:[1;31m(nil)[0;m>\n",
            (void *) r, r->id, *((void **) dp));
        }
#endif
        sp += sizeof(srid_t);
        dp += sizeof(void *);
        break;
      }
      case SKIND_CLASS: {
        *((sclass_t **) dp) = store_get_class(s, *((srid_t *) sp));
#if VERBOSEDEBUG
        if (*((void **) dp)) {
          fprintf(stderr, "    element of <%p:%u> (class) <[1;33m%p[0;m::[1;35m%u[0;m>\n",
            (void *) r, r->id, *((void **) dp), (*((sclass_t **) dp))->id);
        } else {
          fprintf(stderr, "    element of <%p:%u> (class) <[1;33m%p[0;m:[1;31m(nil)[0;m>\n",
            (void *) r, r->id, *((void **) dp));
        }
#endif
        sp += sizeof(srid_t);
        dp += sizeof(void *);
        break;
      }
    }
  }

  err_t err = srid_insert(&s->i2r, r->id, r, 0);
  if (err) {
    gc_error(g, "smrec: attempted to replace id %u", (unsigned) r->id);
  }

  return 0;
}

static
size_t smrec_clear(gc_global_t * g, gc_hdr_t * o)
{
  store_t * s = (store_t *) g;
  smrec_t * r = (smrec_t *) o;

  err_t err = srid_delete(&s->i2r, r->id);
  if (err) {
    gc_error(g, "smrec: attempted to delete non-existent id %u", (unsigned) r->id);
  }

  gc_mem_free(g, r->sz, r->ptr);

  return 0;
}

static
size_t smrec_propagate(gc_global_t * g, gc_obj_t * o)
{
  store_t * s = (store_t *) g;
  smrec_t * r = (smrec_t *) o;
  size_t    k = 0;

  if (r->limb) {
    gc_mark(g, GC_HDR(r->limb)); k++;
  }

  k += sclass_walk_propagate(s, r->sc, r->ptr);

  return k;
}

gc_vtable_t smrec_vtable = {
  .name = "smrec_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = smrec_init,
  .gc_clear = smrec_clear,
  .gc_propagate = smrec_propagate,
};

smrec_t * store_get_object(store_t * s, srid_t id)
{
  sdrec_t r = spman_get(&s->g, &s->pm, id);
  if (r.id == SRID_NIL) {
    return NULL;
  }
  smrec_t * rec = NULL;
  if ((r.flag & SSLOT_USAGEMASK) == SSLOT_USAGE_OBJECT) {
    if (srid_find(&s->i2r, r.id, (void **) &rec)) {
      sclass_t * c = store_get_class(s, *((srid_t *) r.slot));
      if (c) {
        /* pin the page (map) so it does not vanish in the heat of the moment */
        spman_ref(&s->g, &s->pm, r.map);
        rec = gc_new(&s->g, &smrec_vtable, sizeof(smrec_t), 3, c, id, r.slot + sizeof(srid_t));
        spman_unref(&s->g, &s->pm, r.map);
      }
    }
  }
  return rec;
}

/*************************************************************************************************/

#define HEADER_STR0 "STORE000"

store_t * store_init(store_t * s, mema_t a, const char path[])
{
  if (!s || !path) {
    return NULL;
  }

  s->hdr = NULL;

  int fd = open(path, O_RDWR);
  if (fd == -1) {
    fd = open(path, O_CREAT | O_RDWR, S_IRWXU);
    if (fd == -1) {
      return NULL;
    }
    if (ftruncate(fd, 4096)) {
      close(fd);
      unlink(path);
      return NULL;
    }
    s->hdr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s->hdr == MAP_FAILED) {
      close(fd);
      unlink(path);
      return NULL;
    }
    memset(s->hdr, 0, 4096);
    memcpy(s->hdr, HEADER_STR0, 8);
  }

  if (!s->hdr) {
    s->hdr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s->hdr == MAP_FAILED) {
      close(fd);
      return NULL;
    }
  }

  if (memcmp(s->hdr, HEADER_STR0, 8) != 0) {
    munmap(s->hdr, 4096);
    close(fd);
    return NULL;
  }

  if (!spman_init(&s->g, &s->pm, fd, 4096, s->hdr->cnt)) {
    munmap(s->hdr, 4096);
    close(fd);
    return NULL;
  }

  gc_init(&s->g, a);

  if (!trie_init(&s->g.alloc, &s->i2r, 8)) {
    gc_clear(&s->g);
    munmap(s->hdr, 4096);
    close(fd);
    return NULL;
  }

  s->limbs = NULL;

  return s;
}

void store_clear(store_t * s)
{
  gc_clear(&s->g);
  trie_clear(&s->i2r);
  s->limbs = NULL;

  s->hdr->cnt = s->pm.cnt;
  munmap(s->hdr, 4096);

  int fd = spman_clear(&s->g, &s->pm);

  close(fd);
}

sclass_t * store_add_class(store_t * s, uint16_t cnt, skind_t kindv[cnt])
{
  sdrec_t r = spman_add(&s->g, &s->pm, sizeof(uint16_t) + cnt * sizeof(srid_t), SSLOT_USAGE_CLASS);
  if (r.id == SRID_NIL) {
    return NULL;
  }
  uint8_t * p = r.slot;
  *((uint16_t *) p) = cnt;
#if VERBOSEDEBUG
  fprintf(stderr, "[D] class [1;35m%u[0;m of %u elements\n", r.id, *((uint16_t *) p));
#endif
  p += sizeof(uint16_t);
  for (uint16_t k = 0; k < cnt; k++) {
    *((srid_t *) p) = kindv[k];
#if VERBOSEDEBUG
    fprintf(stderr, "    element %u kind: [1;34m%u[0;m\n", k, *((srid_t *) p));
#endif
    p += sizeof(srid_t);
  }

  return store_get_class(s, r.id);
}

smrec_t * store_add_object(store_t * s, sclass_t * c, ...)
{
  va_list ap;
  /*va_start(ap, c);
  va_end(ap);*/

  size_t sz = 0;

  va_start(ap, c);
  for (uint16_t k = 0; k < c->cnt; k++) {
    switch (c->kind[k]) {
      case SKIND_INT32:
        va_arg(ap, int32_t);
        sz += sizeof(int32_t); break;
        break;
      case SKIND_UINT32:
        va_arg(ap, uint32_t);
        sz += sizeof(uint32_t); break;
        break;
      case SKIND_INT64:
        va_arg(ap, int64_t);
        sz += sizeof(int64_t); break;
        break;
      case SKIND_UINT64:
        va_arg(ap, uint64_t);
        sz += sizeof(uint64_t); break;
        break;
      case SKIND_DOUBLE:
        va_arg(ap, double);
        sz += sizeof(double); break;
        break;
      case SKIND_STRING: {
        uint16_t l = va_arg(ap, int);
        const char * a = va_arg(ap, const char *);
        sz += sizeof(uint16_t) + (a ? ALIGN16(l) : 0);
        break;
      }
      case SKIND_OBJECT:
        va_arg(ap, smrec_t *);
        sz += sizeof(srid_t); break;
        break;
      case SKIND_CLASS:
        va_arg(ap, sclass_t *);
        sz += sizeof(srid_t); break;
        break;
    }
  }
  va_end(ap);

  if (sz >= STORE_DATASIZE) {
    gc_error(&s->g, "store: attempted to create an object lager than a data page!");
  }

  sdrec_t r = spman_add(&s->g, &s->pm, (uint16_t) sz, SSLOT_USAGE_OBJECT);
  if (r.id == SRID_NIL) {
    return NULL;
  }

  uint8_t * p = r.slot;

  *((srid_t *) p) = c->id;
#if VERBOSEDEBUG
  fprintf(stderr, "{D} object of class %u\n", *((srid_t *) p));
#endif
  p += sizeof(srid_t);

  va_start(ap, c);
  for (uint16_t k = 0; k < c->cnt; k++) {
    switch (c->kind[k]) {
      case SKIND_INT32:
        *((int32_t *) p) = va_arg(ap, int32_t);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (int32) %d\n", *((int32_t *) p));
#endif
        p += sizeof(int32_t);
        break;
      case SKIND_UINT32:
        *((uint32_t *) p) = va_arg(ap, uint32_t);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (uint32) %u\n", *((uint32_t *) p));
#endif
        p += sizeof(uint32_t);
        break;
      case SKIND_INT64:
        *((int64_t *) p) = va_arg(ap, int64_t);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (int64) %ld\n", (long) *((int64_t *) p));
#endif
        p += sizeof(int64_t);
        break;
      case SKIND_UINT64:
        *((uint64_t *) p) = va_arg(ap, uint64_t);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (uint64) %lu\n", (long unsigned) *((uint64_t *) p));
#endif
        p += sizeof(uint64_t);
        break;
      case SKIND_DOUBLE:
        *((double *) p) = va_arg(ap, double);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (double) %g\n", *((double *) p));
#endif
        p += sizeof(double);
        break;
      case SKIND_STRING: {
        uint16_t l = va_arg(ap, int);
        const char * a = va_arg(ap, const char *);
        *((uint16_t *) p) = l;
        p += sizeof(uint16_t);
        memcpy(p, a, l);
#if VERBOSEDEBUG
        fprintf(stderr, "--- element (string) %.*s\n", l, a);
#endif
        p += ALIGN16(l);
        break;
      }
      case SKIND_OBJECT: {
        smrec_t * t = va_arg(ap, smrec_t *);
        if (t) {
          *((srid_t *) p) = t->id;
#if VERBOSEDEBUG
          fprintf(stderr, "--- element (object) %u\n", *((srid_t *) p));
#endif
        } else {
          *((srid_t *) p) = SRID_NIL;
#if VERBOSEDEBUG
          fprintf(stderr, "--- element (object) (nil)\n");
#endif
        }
        p += sizeof(srid_t);
        break;
      }
      case SKIND_CLASS: {
        sclass_t * t = va_arg(ap, sclass_t *);
        if (t) {
          *((srid_t *) p) = t->id;
#if VERBOSEDEBUG
          fprintf(stderr, "--- element (class) %u\n", *((srid_t *) p));
#endif
        } else {
          *((srid_t *) p) = SRID_NIL;
#if VERBOSEDEBUG
          fprintf(stderr, "--- element (class) (nil)\n");
#endif
        }
        p += sizeof(srid_t);
        break;
      }
    }
  }

  va_end(ap);

  return store_get_object(s, r.id);
}
