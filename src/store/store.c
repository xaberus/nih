#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>

#include "store.h"

#include "store-tests.c"

#define SRID_TO_PAGE(_srid) (((_srid) & STORE_PAGEMASK) >> STORE_SLOTBITS)
#define SRID_TO_SLOT(_srid) ((_srid) & STORE_SLOTMASK)

#define VERBOSEDEBUG 0

#define CLASS_FMT "[1;35m%u[0;m"
#define PTR_FMT "[1;33m%p[0;m"
#define PAGE_FMT "[1;32m%u[0;m"
#define SRID_FMT "[1;31m%u[0;m"
#define INT32_FMT "%d"
#define UINT32_FMT "%u"
#define INT64_FMT "%ld"
#define UINT64_FMT "%lu"

#define ELEMENT(_t, _p) (*((_t *) (_p)))

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

/*************************************************************************************************/

#define ALIGN2(_size) (((_size) + 1L) & ~1L)

/* on disk format */

inline static
uint8_t * sdisk_u16_write(uint8_t * dst, uint16_t value)
{
  *((uint16_t *) dst) = value;
  return dst + sizeof(uint16_t);
}

inline static
uint8_t * sdisk_i32_write(uint8_t * dst, int32_t value)
{
  *((int32_t *) dst) = value;
  return dst + sizeof(int32_t);
}

inline static
uint8_t * sdisk_u32_write(uint8_t * dst, uint32_t value)
{
  *((uint32_t *) dst) = value;
  return dst + sizeof(uint32_t);
}

inline static
uint8_t * sdisk_i64_write(uint8_t * dst, int64_t value)
{
  *((int64_t *) dst) = value;
  return dst + sizeof(int64_t);
}

inline static
uint8_t * sdisk_u64_write(uint8_t * dst, uint64_t value)
{
  *((uint64_t *) dst) = value;
  return dst + sizeof(uint64_t);
}

inline static
uint8_t * sdisk_dbl_write(uint8_t * dst, double value)
{
  *((double *) dst) = value;
  return dst + sizeof(double);
}

inline static
uint8_t * sdisk_str_write(uint8_t * dst, uint16_t len, const char value[len])
{
  uint32_t skip = ALIGN2(len) - len;
  dst = sdisk_u16_write(dst, len);
  memcpy(dst, value, len);
  dst += len;
  if (skip) {
    memset(dst, 0, skip);
  }
  return dst + skip;
}

inline static
uint8_t * sdisk_obj_write(uint8_t * dst, smrec_t * o)
{
  if (o) {
    return sdisk_u32_write(dst, o->id);
  } else {
    return sdisk_u32_write(dst, SRID_NIL);
  }
}

inline static
uint8_t * sdisk_cls_write(uint8_t * dst, sclass_t * c)
{
  if (c) {
    return sdisk_u32_write(dst, c->id);
  } else {
    return sdisk_u32_write(dst, SRID_NIL);
  }
}


/*************************************************************************************************/

spman_t * spman_init(spman_t * pm, int fd, off_t offset, uint32_t cnt)
{
  if (!pm || fd == -1)
    return NULL;

  pm->fd = fd;
  pm->offset = offset;
  pm->cnt = cnt;
  pm->anum = 0;
  pm->maps = 0;
  memset(pm->map, 0, sizeof(spmap_t *) * STORE_LIVEPAGES);

  return pm;
}

int spman_clear(spman_t * pm)
{
  int fd = pm->fd;

  for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
    spmap_t * m = pm->map[k], * n;
    while (m) {
      n = m->next;
      spman_unref(pm, m);
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

#define SPAGE_HDRWORDS 2
#define SPAGE_HDRSIZE (sizeof(uint16_t) * SPAGE_HDRWORDS)
#define SPAGE_HDR_PN(_p) (*((uint16_t *) (_p)))
#define SPAGE_HDR_SCOUNT(_p) (*((uint16_t *) (_p) + 1))
#define SPAGE_HDR_DATA(_p) ((void *)(((uint16_t *) (_p) + SPAGE_HDRWORDS)))

#define SSLOT_HDRWORDS 2
#define SSLOT_HDRSIZE (sizeof(uint16_t) * SSLOT_HDRWORDS)
#define SSLOT_HDR_FLAG(_p) (*((uint16_t *) (_p)))
#define SSLOT_HDR_SIZE(_p) (*((uint16_t *) (_p) + 1))
#define SSLOT_HDR_SLOT(_p) ((void *)(((uint16_t *) (_p) + SSLOT_HDRWORDS)))
#define SSLOT_HDR_NEXT(_p, _size) ((uint8_t *) SSLOT_HDR_SLOT(_p) + ALIGN2(_size))

static
void spmap_gen_index(spmap_t * m)
{
  memset(m->index, 0, sizeof(m->index));
  void * p = SPAGE_HDR_DATA(m->page);
  for (uint32_t sid = 0; sid < m->scount; sid++) {
    uint16_t sflag = SSLOT_HDR_FLAG(p);
    uint16_t ssize = SSLOT_HDR_SIZE(p);
    void * slot = SSLOT_HDR_SLOT(p);
    // fprintf(stderr, "IDX %08u|%04u|%04u|%04u|%p\n", p - m->page, sid, sflag, ALIGN2(ssize), slot);
    if (sid < STORE_SLOTSMAX) {
      sslot_t * s = &m->index[sid];
      s->flag = sflag;
      s->size = ssize;
      s->slot = slot;
      if (sflag & SSLOT_FLAG_FLST) {
        m->sfree = 1;
        m->sfhdr = sid;
      }
    }
    p = SSLOT_HDR_NEXT(p, ssize);
  }
}

static
sdrec_t spmap_alloc(spmap_t * m, uint16_t ssize, uint16_t usage)
{
  if (m->sfree) {

  }

  if (m->scount < STORE_SLOTSMAX) {
    uint16_t sid = m->scount;
    uint32_t sz = ALIGN2(ssize);
    uint32_t off;
    if (!sid) {
      off = SPAGE_HDRSIZE;
    } else {
      off = (uint8_t *) m->index[sid - 1].slot - (uint8_t *) m->page;
      off += ALIGN2(m->index[sid - 1].size);
    }
    if (off + sz + SSLOT_HDRSIZE <= m->psize) {
      void * p = (uint8_t *) m->page + off;
      uint16_t sflag = SSLOT_FLAG_DATA | (usage & SSLOT_USAGEMASK);
      SSLOT_HDR_FLAG(p) = sflag;
      SSLOT_HDR_SIZE(p) = ssize;
      void * slot = SSLOT_HDR_SLOT(p);
      // fprintf(stderr, "OFF %08u|%04u|%04u|%04u\n", off, sid, sflag| ssize);
      sslot_t * s = &m->index[sid];
      s->flag = sflag;
      s->size = ssize;
      s->slot = slot;
      m->scount++;
      return (sdrec_t) {
        .id = (m->pnum << STORE_SLOTBITS) | sid,
        .size = ssize,
        .flag = sflag,
        .slot = slot,
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


spmap_t * spman_load(spman_t * pm, uint32_t pnum)
{
  if (pnum >= pm->cnt) {
    /* no page in file */
    return NULL;
  }

  { /* try to find loaded page */
    uint32_t  hash = hash32(pnum) & STORE_LIVEMASK;
    spmap_t * c = pm->map[hash];
    while (c) {
      if (c->pnum == pnum) {
        return c;
      }
      c = c->next;
    }
  }

  /* drop loaded pages as needed */
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
      spman_unref(pm, del);
    }
  }

  /* going to load page now! */
  off_t off = pm->offset;
  uint16_t pn;
  for (uint32_t k = 0; k <= pnum; k++) {
    if (lseek(pm->fd, off, SEEK_SET) == (off_t) -1) {
      return NULL;
    }
    if (read(pm->fd, &pn, sizeof(pn)) != sizeof(pn)) {
      return NULL;
    }
    if (k < pnum) {
      off += pn * STORE_DATACHUNK;
    }
  }
  uint16_t scount;
  if (read(pm->fd, &scount, sizeof(scount)) != sizeof(scount)) {
    return NULL;
  }
  uint32_t psize = pn * STORE_DATACHUNK;
  void * b = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_SHARED, pm->fd, off);
  if (b == MAP_FAILED)
    return NULL;

  spmap_t * m = malloc(sizeof(spmap_t));
  if (!m) {
    munmap(b, psize);
    return NULL;
  }
  m->pnum = pnum;
  m->inuse = 0;
  m->ref = 0;
  m->scount = scount;
  m->psize = psize;
  m->page = b;
  m->poff = off;
  m->sfree = 0;
  m->sfhdr = 0;

#if VERBOSEDEBUG > 0
  fprintf(stderr, "[P] loaded page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes}\n",
    (void *) m->page, pnum, scount, psize);
#endif

  spmap_gen_index(m);

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

  return spman_ref(pm, m);;
}

void spman_unload(spman_t * pm, spmap_t * m)
{
    *(m->rev) = m->next;
    if (m->next) {
      m->next->rev = m->rev;
    }
    uint16_t * p = m->page;
    *p = m->psize / STORE_DATACHUNK; p++;
    *p = m->scount;
#if VERBOSEDEBUG > 0
    fprintf(stderr, "{P} unloading page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes}\n",
      (void *) m->page, m->pnum, m->scount, m->psize);
#endif
    munmap(m->page, m->psize);
    free(m);
    pm->maps--;
}

spmap_t * spman_ref(spman_t * pm, spmap_t * m)
{
  (void) pm;
  m->ref++;
  return m;
}

void spman_unref(spman_t * pm, spmap_t * m)
{
  if (m->ref > 1) {
    m->ref--;
  } else {
    spman_unload( pm, m);
  }
}

sdrec_t spman_add(spman_t * pm, uint16_t size, uint16_t usage)
{
  spmap_t * m;

  while ((m = spman_load(pm, pm->anum))) {
    sdrec_t r = spmap_alloc(m, size, usage);
    if (r.id != SRID_NIL) {
      return r;
    }
    pm->anum++;
  }

#if VERBOSEDEBUG > 2
      fprintf(stderr, "[P] resizing store to %u pages (anum was "PAGE_FMT")\n",
        pm->cnt + 1, pm->anum);
#endif

  /* no free slots found till now, so get offset from last page and append a new */
  off_t off = (off_t) -1;
  if (pm->cnt) {
    m = spman_load(pm, pm->cnt - 1);
    off = m->poff + m->psize;
  } else {
    off = pm->offset;
  }
  if (off != (off_t) -1) {
    uint16_t pn = 16;
    uint32_t psize = (pn * STORE_DATACHUNK);
    if (!ftruncate(pm->fd, off + psize)) {
#if VERBOSEDEBUG > 2
      fprintf(stderr, "[P] appended %u bytes to store\n", psize);
#endif
      if (lseek(pm->fd, off, SEEK_SET) != (off_t) -1) {
        uint16_t nil = pn;
        if (write(pm->fd, &nil, sizeof(nil)) == sizeof(nil)) {
          nil = 0;
          if (write(pm->fd, &nil, sizeof(nil)) == sizeof(nil)) {
            pm->cnt++;
            if ((m = spman_load(pm, pm->anum))) {
              return spmap_alloc(m, size, usage);
            }
          }
        }
      }
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

sdrec_t spman_get(spman_t * pm, srid_t id)
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
    m = spman_load(pm, pnum);
  }

  if (m) {
    if (m->index[snum].flag & SSLOT_FLAG_DATA) {
      return (sdrec_t) {
        .id = id,
        .size = m->index[snum].size,
        .flag = m->index[snum].flag,
        .slot = m->index[snum].slot,
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

  if (argc != 4) {
    gc_error(g, "sclass: init without arguments");
  }

  c->id = va_arg(ap, srid_t);
  c->meta = va_arg(ap, smrec_t *);
  c->fcnt = va_arg(ap, int);

  uint8_t * p = va_arg(ap, uint8_t *);

  for (uint16_t k = 0; k < c->fcnt; k++) {
    c->flds[k].kind = ELEMENT(uint16_t, p);
    p += sizeof(uint16_t);
    c->flds[k].meta = store_get_object(s, ELEMENT(srid_t, p));
    p += sizeof(srid_t);
  }

  err_t err = srid_insert(&s->i2r, c->id, c, 0);
  if (err) {
    gc_error(g, "sclass: attempted to replace id %u", (unsigned) c->id);
  }

  spmap_t * m = spman_load(&s->pm, SRID_TO_PAGE(c->id));
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

  spmap_t * m = spman_load(&s->pm, SRID_TO_PAGE(c->id));
  if (!m) {
    gc_error(g, "sclass: could not load page %u", (unsigned) SRID_TO_PAGE(c->id));
  }
  m->inuse--;

  return 0;
}

static
size_t sclass_propagate(gc_global_t * g, gc_obj_t * o)
{
  sclass_t * c = (sclass_t *) o;
  size_t n = 0;

  if (c->meta) {
    gc_mark(g, GC_HDR(c->meta)); n++;
  }

  for (uint16_t k = 0; k < c->fcnt; k++) {
    smrec_t * meta = c->flds[k].meta;
    if (meta) {
      gc_mark(g, GC_HDR(meta)); n++;
    }
  }
  return n;
}

gc_vtable_t sclass_vtable = {
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = sclass_init,
  .gc_clear = sclass_clear,
  .gc_propagate = sclass_propagate,
};

sclass_t * store_get_class(store_t * s, srid_t id)
{
  sdrec_t r = spman_get(&s->pm, id);
  if (r.id == SRID_NIL) {
    return NULL;
  }

  void * c = NULL;
  if ((r.flag & SSLOT_USAGEMASK) == SSLOT_USAGE_CLASS) {
    if (srid_find(&s->i2r, r.id, &c)) {
      uint8_t * p = r.slot;
      smrec_t * meta = store_get_object(s, ELEMENT(srid_t, p)); p += sizeof(srid_t);
      uint16_t fcnt = ELEMENT(uint16_t, p); p += sizeof(uint16_t);
      spman_ref(&s->pm, r.map);
      c = gc_new(&s->g, &sclass_vtable, sizeof(sclass_t) + sizeof(scfld_t) * fcnt,
            4, id, meta, fcnt, p);
      spman_unref(&s->pm, r.map);
    }
  }
  return c;
}

inline static
uint16_t sclass_instargc(sclass_t * c)
{
  uint16_t r = 0;
  for (uint16_t k = 0; k < c->fcnt; k++) {
    switch (c->flds[k].kind) {
      case SKIND_INT32: r++; break;
      case SKIND_UINT32: r++; break;
      case SKIND_INT64: r++; break;
      case SKIND_UINT64: r++; break;
      case SKIND_DOUBLE: r++; break;
      case SKIND_STRING: r += 2; break;
      case SKIND_OBJECT: r++; break;
      case SKIND_CLASS: r++; break;
    }
  }
  return r;
}

uint16_t sclass_mem_size(sclass_t * c)
{
  uint16_t sz = 0;
  for (uint16_t k = 0; k < c->fcnt; k++) {
    switch (c->flds[k].kind) {
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
  for (uint16_t k = 0; k < c->fcnt; k++) {
    switch (c->flds[k].kind) {
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

#if VERBOSEDEBUG
#define MCOPYP(_t, _d, _s, _f, _r) \
  do { \
    _t val = ELEMENT(_t, _s); (_s) += sizeof(_t); \
    ELEMENT(_t, _d) = val; (_d) += sizeof(_t); \
    fprintf(stderr, "    element of <%p:%u> (" #_t ") " _f "\n", \
      (void *) r, r ? r->id : SRID_NIL, val);\
  } while(0)
#else
#define MCOPYP(_t, _d, _s, _f, _r) \
  do { \
    _t val = ELEMENT(_t, _s); (_s) += sizeof(_t); \
    ELEMENT(_t, _d) = val; (_d) += sizeof(_t); \
  } while(0)
#endif

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

  r->ptr = malloc(r->sz);

#if VERBOSEDEBUG
  fprintf(stderr, "{M} object <"PTR_FMT":"SRID_FMT"> of class <"PTR_FMT":"CLASS_FMT">\n",
    (void *) r, r->id, (void *) r->sc, r->sc->id);
#endif

  uint8_t * sp = slot;
  uint8_t * dp = r->ptr;
  for (uint16_t k = 0; k < r->sc->fcnt; k++) {
    switch (r->sc->flds[k].kind) {
      case SKIND_INT32:
        MCOPYP(int32_t, dp, sp, INT32_FMT, r); break;
      case SKIND_UINT32:
        MCOPYP(uint32_t, dp, sp, UINT32_FMT, r); break;
      case SKIND_INT64:
        MCOPYP(int64_t, dp, sp, INT64_FMT, r); break;
      case SKIND_UINT64:
        MCOPYP(uint64_t, dp, sp, UINT64_FMT, r); break;
      case SKIND_DOUBLE:
        MCOPYP(double, dp, sp, "%g", r); break;
      case SKIND_STRING: {
        uint16_t l = ELEMENT(uint16_t, sp);
        sp += sizeof(uint16_t);
        gc_str_t * str = gc_new_str(&s->g, l, (char *) sp);
        ELEMENT(gc_str_t *, dp) = str;
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (string) <%.*s>\n",
          (void *) r, r->id, gc_str_len(str), str->data);
#endif
        sp += ALIGN2(l);
        dp += sizeof(gc_str_t *);
        break;
      }
      case SKIND_OBJECT: {
        smrec_t * val = store_get_object(s, ELEMENT(srid_t, sp));
        sp += sizeof(srid_t);
        ELEMENT(smrec_t *, dp) =  val;
        dp += sizeof(smrec_t *);
#if VERBOSEDEBUG
        if (val) {
          fprintf(stderr, "    element of <%p:%u> (object) <"PTR_FMT":"SRID_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) val, val->id, val->sc->id);
        } else {
          fprintf(stderr, "    element of <%p:%u> (object) <"PTR_FMT">\n",
            (void *) r, r->id, (void *) val);
        }
#endif
        break;
      }
      case SKIND_CLASS: {
        sclass_t * val = store_get_class(s, ELEMENT(srid_t, sp));
        sp += sizeof(srid_t);
        ELEMENT(sclass_t *, dp) = val;
        dp += sizeof(sclass_t *);
#if VERBOSEDEBUG
        if (ELEMENT(sclass_t *, dp)) {
          fprintf(stderr, "    element of <%p:%u> (class) <"PTR_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) val, val->id);
        } else {
          fprintf(stderr, "    element of <%p:%u> (class) <"PTR_FMT">\n",
            (void *) r, r->id, (void *) val);
        }
#endif
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

  free(r->ptr);

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
  sdrec_t r = spman_get(&s->pm, id);
  if (r.id == SRID_NIL) {
    return NULL;
  }
  smrec_t * rec = NULL;
  if ((r.flag & SSLOT_USAGEMASK) == SSLOT_USAGE_OBJECT) {
    if (srid_find(&s->i2r, r.id, (void **) &rec)) {
      sclass_t * c = store_get_class(s, *((srid_t *) r.slot));
      if (c) {
        /* pin the page (map) so it does not vanish in the heat of the moment */
        spman_ref(&s->pm, r.map);
        rec = gc_new(&s->g, &smrec_vtable, sizeof(smrec_t), 3, c, id, r.slot + sizeof(srid_t));
        spman_unref(&s->pm, r.map);
      }
    }
  }
  return rec;
}

/*************************************************************************************************/

#define HEADER_STR0 "STORE000"

store_t * store_init(store_t * s, const char path[])
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

  if (!spman_init(&s->pm, fd, 4096, s->hdr->cnt)) {
    munmap(s->hdr, 4096);
    close(fd);
    return NULL;
  }

  gc_init(&s->g);

  if (!trie_init(&s->i2r, 8)) {
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

  int fd = spman_clear(&s->pm);

  close(fd);
}

sclass_t * store_add_class(store_t * s, smrec_t * meta, uint16_t fcnt, scfld_t flds[fcnt])
{
  uint32_t sz = sizeof(srid_t) + sizeof(uint16_t) + fcnt * (sizeof(uint16_t) + sizeof(srid_t));
  if (sz >= STORE_DATASIZE) {
    gc_error(&s->g, "store: attempted to create a class lager than DATASIZE!");
  }

  sdrec_t r = spman_add(&s->pm, sz, SSLOT_USAGE_CLASS);
  if (r.id == SRID_NIL) {
    return NULL;
  }

  uint8_t * p = r.slot;

#if VERBOSEDEBUG
  fprintf(stderr, "[D] writing class [1;35m%u[0;m of %u elements {%u bytes}\n",
    r.id, fcnt, sz);
#endif

  p = sdisk_obj_write(p, meta);
  p = sdisk_u16_write(p, fcnt);

  for (uint16_t k = 0; k < fcnt; k++) {
    p = sdisk_u16_write(p, flds[k].kind);
    p = sdisk_obj_write(p, flds[k].meta);
  }

  return store_get_class(s, r.id);
}

smrec_t * store_add_object(store_t * s, sclass_t * c, ...)
{
  va_list ap;
  /*va_start(ap, c);
  va_end(ap);*/

  uint32_t sz = sizeof(srid_t);

  union {
    uint16_t     u16;
    int32_t      i32;
    uint32_t     u32;
    int64_t      i64;
    uint64_t     u64;
    double       dbl;
    const char * str;
    smrec_t    * obj;
    sclass_t   * cls;
  } args[sclass_instargc(c)];

  va_start(ap, c);
  for (uint16_t k = 0, v = 0; k < c->fcnt; k++) {
    switch (c->flds[k].kind) {
      case SKIND_INT32:
        args[v++].i32 = va_arg(ap, int32_t); sz += sizeof(int32_t); break;
      case SKIND_UINT32:
        args[v++].u32 = va_arg(ap, uint32_t); sz += sizeof(uint32_t); break;
      case SKIND_INT64:
        args[v++].i64 = va_arg(ap, int64_t); sz += sizeof(int64_t); break;
      case SKIND_UINT64:
        args[v++].u64 = va_arg(ap, uint64_t); sz += sizeof(uint64_t); break;
      case SKIND_DOUBLE:
        args[v++].dbl = va_arg(ap, double); sz += sizeof(double); break;
      case SKIND_STRING: {
        uint16_t l = va_arg(ap, int);
        const char * a = va_arg(ap, const char *);
        args[v++].u16 = l; sz += sizeof(uint16_t);
        args[v++].str = a; sz += (a ? ALIGN2(l) : 0);
        break;
      }
      case SKIND_OBJECT:
        args[v++].obj = va_arg(ap, smrec_t *); sz += sizeof(srid_t); break;
      case SKIND_CLASS:
        args[v++].cls = va_arg(ap, sclass_t *); sz += sizeof(srid_t); break;
    }
  }
  va_end(ap);

  if (sz >= STORE_DATASIZE) {
    gc_error(&s->g, "store: attempted to create an object lager than DATASIZE!");
  }

  sdrec_t r = spman_add(&s->pm, sz, SSLOT_USAGE_OBJECT);
  if (r.id == SRID_NIL) {
    return NULL;
  }

  uint8_t * p = r.slot;

#if VERBOSEDEBUG
  fprintf(stderr, "{D} writing object of class %u with {%u elements, %u bytes}\n", c->id, c->fcnt, sz);
#endif

  /* write class reference first */
  p = sdisk_cls_write(p, c);

  for (uint16_t k = 0, v = 0; k < c->fcnt; k++) {
    switch (c->flds[k].kind) {
      case SKIND_INT32:
        p = sdisk_i32_write(p, args[v++].i32); break;
      case SKIND_UINT32:
        p = sdisk_u32_write(p, args[v++].u32); break;
      case SKIND_INT64:
        p = sdisk_i64_write(p, args[v++].i64); break;
      case SKIND_UINT64:
        p = sdisk_u64_write(p, args[v++].u64); break;
      case SKIND_DOUBLE:
        p = sdisk_dbl_write(p, args[v++].dbl); break;
      case SKIND_STRING:
        p = sdisk_str_write(p, args[v].u16, args[v+1].str); v += 2; break;
      case SKIND_OBJECT:
        p = sdisk_obj_write(p, args[v++].obj); break;
      case SKIND_CLASS: {
        p = sdisk_cls_write(p, args[v++].cls); break;
      }
    }
  }

  return store_get_object(s, r.id);
}
