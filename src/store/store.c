#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>

#include "store.h"

/*
  this is almost Jenkins reversible 32bit hash mix function
  from http://burtleburtle.net/bob/hash/integer.html
  (I could not find the inverse operation to the original 4th assignment)
*/
uint32_t hash32(uint32_t a)
{
  a = (a + (a << 12)) + 0x7ed55d16; /* a+a<<12 = a*4097 */
  a = (a^0xc761c23c)  ^ (a>>19);
  a = (a + (a << 5))  + 0x165667b1; /* a+a<<5 = a*33 */
  a = (a + (a << 9))  ^ 0xd3a2646c; /* a+a<<9 = a*513 */
  a = (a + (a << 3))  + 0xfd7046c5; /* a+a<<3 = a*9 */
  a = (a^0xb55a4f09)  ^ (a>>16);

  return a;
}

/* this is the corresponding inverse hash mix function */
uint32_t ihash32(uint32_t a)
{
  a ^= 0xb55a4f09; a ^= (a>>16);
  a -= 0xfd7046c5; a *= 0x38e38e39;  /* modinv32(9) == 0x38e38e39 */
  a ^= 0xd3a2646c; a *= 0xf803fe01;  /* modinv32(513) == 0xf803fe01 */
  a -= 0x165667b1; a *= 0x3e0f83e1;  /* modinv32(33) == 0x3e0f83e1 */
  a ^= 0xc761c23c; a ^= (a>>19);
  a -= 0x7ed55d16; a *= 0xfff001;    /* modinv32(4097) == 0xfff001 */

  return a;
}

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

/*************************************************************************************************/

#define ALIGN2(_size) (((_size) + 1L) & ~1L)
#define ALIGNDATA(_size) (((_size) + (STORE_DATACHUNK-1)) & ~(STORE_DATACHUNK-1))

/* on disk format */

inline static
uint8_t * sdisk_u16_write(uint8_t * dst, uint16_t value)
{
  *((uint16_t *) dst) = value;
  return dst + sizeof(uint16_t);
}

inline static
uint16_t sdisk_u16_read(uint8_t ** dstp)
{
  uint16_t value = *((uint16_t *) *dstp);
  *dstp += sizeof(uint16_t);
  return value;
}

inline static
uint8_t * sdisk_i32_write(uint8_t * dst, int32_t value)
{
  *((int32_t *) dst) = value;
  return dst + sizeof(int32_t);
}

inline static
int32_t sdisk_i32_read(uint8_t ** dstp)
{
  int32_t value = *((int32_t *) *dstp);
  *dstp += sizeof(int32_t);
  return value;
}

inline static
uint8_t * sdisk_u32_write(uint8_t * dst, uint32_t value)
{
  *((uint32_t *) dst) = value;
  return dst + sizeof(uint32_t);
}

inline static
uint32_t sdisk_u32_read(uint8_t ** dstp)
{
  uint32_t value = *((uint32_t *) *dstp);
  *dstp += sizeof(uint32_t);
  return value;
}

inline static
uint8_t * sdisk_i64_write(uint8_t * dst, int64_t value)
{
  *((int64_t *) dst) = value;
  return dst + sizeof(int64_t);
}

inline static
int64_t sdisk_i64_read(uint8_t ** dstp)
{
  int64_t value = *((int64_t *) *dstp);
  *dstp += sizeof(int64_t);
  return value;
}

inline static
uint8_t * sdisk_u64_write(uint8_t * dst, uint64_t value)
{
  *((uint64_t *) dst) = value;
  return dst + sizeof(uint64_t);
}

inline static
uint64_t sdisk_u64_read(uint8_t ** dstp)
{
  uint64_t value = *((uint64_t *) *dstp);
  *dstp += sizeof(uint64_t);
  return value;
}

inline static
uint8_t * sdisk_dbl_write(uint8_t * dst, double value)
{
  *((double *) dst) = value;
  return dst + sizeof(double);
}

inline static
double sdisk_dbl_read(uint8_t ** dstp)
{
  double value = *((double *) *dstp);
  *dstp += sizeof(double);
  return value;
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

inline static
uint8_t * smem_i32_write(uint8_t * dst, int32_t value)
{
  *((int32_t *) dst) = value;
  return dst + sizeof(int32_t);
}

inline static
uint8_t * smem_u32_write(uint8_t * dst, uint32_t value)
{
  *((uint32_t *) dst) = value;
  return dst + sizeof(uint32_t);
}

inline static
uint8_t * smem_i64_write(uint8_t * dst, int64_t value)
{
  *((int64_t *) dst) = value;
  return dst + sizeof(int64_t);
}

inline static
uint8_t * smem_u64_write(uint8_t * dst, uint64_t value)
{
  *((uint64_t *) dst) = value;
  return dst + sizeof(uint64_t);
}

inline static
uint8_t * smem_dbl_write(uint8_t * dst, double value)
{
  *((double *) dst) = value;
  return dst + sizeof(double);
}

inline static
uint8_t * smem_ptr_write(uint8_t * dst, void * value)
{
  *((void **) dst) = value;
  return dst + sizeof(void *);
}

/*************************************************************************************************/

err_r * spman_init(spman_t * pm, int fd, off_t offset, uint32_t cnt)
{
  if (fd == -1) {
    return err_return(ERR_IN_INVALID, "invalid file descriptor passed");
  }

  pm->fd = fd;
  pm->offset = offset;
  pm->cnt = cnt;
  pm->anum = 0;
  pm->maps = 0;
  memset(pm->map, 0, sizeof(spmap_t *) * STORE_LIVEPAGES);

  return NULL;
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

#define SSLOT_HDRWORDS 2
#define SSLOT_HDRSIZE (sizeof(uint16_t) * SSLOT_HDRWORDS)


static
void spmap_gen_index(spmap_t * m)
{
  memset(m->index, 0, sizeof(m->index));
  uint8_t * p = m->page;
  sdisk_u16_read(&p); /* pn */
  sdisk_u16_read(&p); /* scount */
  for (uint32_t sid = 0; sid < m->scount; sid++) {
    uint16_t sflag = sdisk_u16_read(&p);
    uint16_t ssize = sdisk_u16_read(&p);
    // fprintf(stderr, "IDX %08u|%04u|%04u|%04u|%p\n", p - m->page, sid, sflag, ALIGN2(ssize), slot);
    if (sid < STORE_SLOTSMAX) {
      sslot_t * s = &m->index[sid];
      s->flag = sflag;
      s->size = ssize;
      s->slot = p;
      if (sflag & SSLOT_FLAG_FLST) {
        m->sfree = 1;
        m->sfhdr = sid;
      }
    }
    p += ssize;
  }
}

static
sdrec_t spmap_alloc(spman_t * pm, spmap_t * m, uint16_t ssize, uint16_t usage)
{
  if (m->sfree) {
    // TODO: implement freelists and deletion
  }

  uint16_t sid = m->scount;
  uint32_t sz = ALIGN2(ssize);
  uint32_t off;
  void * p;
  uint16_t sflag, * wp;
  if (!sid) {
    off = SPAGE_HDRSIZE;
  } else {
    off = (uint8_t *) m->index[sid - 1].slot - (uint8_t *) m->page;
    off += ALIGN2(m->index[sid - 1].size);
  }
  uint32_t needed = sz + SSLOT_HDRSIZE;
  if (off + needed > m->psize) {
    uint32_t npsize = off + needed;
    /* check for overflow */
    if (npsize > m->psize) {
      if (spman_truncate(pm, m, npsize)) {
        err_reset();
      }
      if (off + sz + SSLOT_HDRSIZE <= m->psize) {
        goto do_alloc;
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
do_alloc:
  p = (uint8_t *) m->page + off;
  sflag = SSLOT_FLAG_DATA | (usage & SSLOT_USAGEMASK);
  p = sdisk_u16_write(p, sflag);
  p = sdisk_u16_write(p, ssize);
  // fprintf(stderr, "OFF %08u|%04u|%04u|%04u\n", off, sid, sflag| ssize);
  sslot_t * s = &m->index[sid];
  s->flag = sflag;
  s->size = ssize;
  s->slot = p;
  m->scount++;
  wp = m->page; wp++;
  *wp = m->scount;
  return (sdrec_t) {
    .id = (m->pnum << STORE_SLOTBITS) | sid,
    .size = ssize,
    .flag = sflag,
    .slot = p,
    .map = m,
  };
}


e_spmap_t spman_load(spman_t * pm, uint32_t pnum)
{
  if (pnum >= pm->cnt) {
    /* no page in file */
    return (e_spmap_t) {err_return(ERR_OVERFLOW, "page number > pages managed"), NULL};
  }

  { /* try to find loaded page */
    uint32_t  hash = hash32(pnum) & STORE_LIVEMASK;
    spmap_t * c = pm->map[hash];
    while (c) {
      if (c->pnum == pnum) {
        return (e_spmap_t) {NULL, c};
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
  uint8_t phdr[SPAGE_HDRSIZE], * h;
  uint16_t pn;
  for (uint32_t k = 0; k <= pnum; k++) {
    if (lseek(pm->fd, off, SEEK_SET) == (off_t) -1) {
      return (e_spmap_t) {err_return(ERR_INVALID, "could not lseek to page start"), NULL};
    }
    if (read(pm->fd, phdr, sizeof(phdr)) != sizeof(phdr)) {
      return (e_spmap_t) {err_return(ERR_INVALID, "could not read page header"), NULL};
    }
    h = phdr;
    pn = sdisk_u16_read(&h);
    if (k < pnum) {
      off += pn * STORE_DATACHUNK;
    }
  }
  uint16_t scount = sdisk_u16_read(&h);
  uint32_t psize = pn * STORE_DATACHUNK;
  void * b = mmap(NULL, psize, PROT_READ | PROT_WRITE, MAP_SHARED, pm->fd, off);
  if (b == MAP_FAILED)
    return (e_spmap_t) {err_return(ERR_INVALID, "could not mmap() page"), NULL};

  spmap_t * m = malloc(sizeof(spmap_t));
  if (!m) {
    munmap(b, psize);
    return (e_spmap_t) {err_return(ERR_MEM_ALLOC, "could not allocate index for page"), NULL};
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

  return (e_spmap_t) {NULL, spman_ref(pm, m)};
}

void spman_unload(spman_t * pm, spmap_t * m)
{
    *(m->rev) = m->next;
    if (m->next) {
      m->next->rev = m->rev;
    }
#if VERBOSEDEBUG > 0
    fprintf(stderr, "{P} unloading page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes}\n",
      (void *) m->page, m->pnum, m->scount, m->psize);
#endif
    munmap(m->page, m->psize);
    free(m);
    pm->maps--;
}

err_r * spman_truncate(spman_t * pm, spmap_t * m, uint32_t psize)
{
  if (pm->cnt) {
    if (pm->cnt - 1 == m->pnum) {
      if (psize > m->psize) {
        uint32_t npsize = ALIGNDATA(psize);
        uint32_t npn = npsize / STORE_DATACHUNK;
        off_t poff = m->poff;
        if (m->scount < STORE_SLOTSMAX) {
          if (ftruncate(pm->fd, poff + npsize)) {
            return err_return(ERR_FAILURE, "could not ftruncate space for new page");
          }
          void * b = mremap(m->page, m->psize, npsize, MREMAP_MAYMOVE, MAP_SHARED);
          if (b == MAP_FAILED) {
            return err_return(ERR_INVALID, "could not mmap() page");
          }
          sdisk_u16_write(b, npn);
          m->page = b;
          m->psize = npsize;
          spmap_gen_index(m);
          return NULL;
        }
      }
    }
  }
  return err_return(ERR_FAILURE, "page truncation failed");
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

e_sdrec_t spman_add(spman_t * pm, uint16_t size, uint16_t usage)
{
  e_spmap_t e;
  err_r * err = NULL;

  for (uint32_t p = pm->anum; p < pm->cnt; p++) {
    e = spman_load(pm, p);
    if (e.err) {
      err = err_return(ERR_CORRUPTION, "could not load page");  goto out;
    }
    sdrec_t r = spmap_alloc(pm, e.spmap, size, usage);
    if (r.id != SRID_NIL) {
      pm->anum = p;
      return (e_sdrec_t) {NULL, r};
    }
  }

#if VERBOSEDEBUG > 2
      fprintf(stderr, "[P] resizing store to %u pages (anum was "PAGE_FMT")\n",
        pm->cnt + 1, pm->anum);
#endif

  /* no free slots found till now, so get offset from last page and append a new */
  off_t off = (off_t) -1;
  if (pm->cnt) {
    e = spman_load(pm, pm->cnt - 1);
    if (e.err) {
      err = err_return(ERR_INVALID, "could not load last page");  goto out;
    }
    off = e.spmap->poff + e.spmap->psize;
  } else {
    off = pm->offset;
  }
  if (off == (off_t) -1) {
    err = err_return(ERR_CORRUPTION, "invalid append offset calculated");  goto out;
  }
  uint16_t pn = STORE_NEWPN;
  uint32_t psize = (pn * STORE_DATACHUNK);
  if (ftruncate(pm->fd, off + psize)) {
    err = err_return(ERR_FAILURE, "could not ftruncate space for new page");  goto out;
  }
#if VERBOSEDEBUG > 2
  fprintf(stderr, "[P] appended %u bytes to store\n", psize);
#endif
  if (lseek(pm->fd, off, SEEK_SET) == (off_t) -1) {
    err = err_return(ERR_FAILURE, "could not lseek to start of new page");  goto out;
  }
  uint8_t phdr[SPAGE_HDRSIZE], * h = phdr;
  h = sdisk_u16_write(h, pn);
  h = sdisk_u16_write(h, 0);
  if (write(pm->fd, phdr, sizeof(phdr)) != sizeof(phdr)) {
    err = err_return(ERR_FAILURE, "could not write page header");  goto out;
  }
  pm->anum = pm->cnt++;
  e = spman_load(pm, pm->anum);
  if (e.err) {
    err = err_return(ERR_INVALID, "could not load new page");  goto out;
  }
  sdrec_t r = spmap_alloc(pm, e.spmap, size, usage);
  if (r.id == SRID_NIL) {
    err = err_return(ERR_CORRUPTION, "slot allocation failed");  goto out;
  }

  return (e_sdrec_t) {NULL, r};

out:
  return (e_sdrec_t) {
    .err = err,
    .sdrec = {
      .id = SRID_NIL,
      .size = 0,
      .flag = 0,
      .slot = NULL,
      .map = NULL,
    },
  };
}

e_sdrec_t spman_get(spman_t * pm, srid_t id)
{
  uint32_t pnum = SRID_TO_PAGE(id);
  uint16_t snum = SRID_TO_SLOT(id);
  uint32_t hash = hash32(pnum) & STORE_LIVEMASK;
  spmap_t * m = pm->map[hash];
  err_r * err = NULL;

  while (m) {
    if (m->pnum == pnum) {
      break;
    }
    m = m->next;
  }

  if (!m) {
    e_spmap_t e = spman_load(pm, pnum);
    if (e.err) {
      err = err_return(ERR_CORRUPTION, "could not load page");  goto out;
    }
    m = e.spmap;
  }

  if (!(m->index[snum].flag & SSLOT_FLAG_DATA)) {
    err = err_return(ERR_INVALID, "requested slot is not initialized");  goto out;
  }

  return (e_sdrec_t) {
    .err = err,
    .sdrec = {
      .id = id,
      .size = m->index[snum].size,
      .flag = m->index[snum].flag,
      .slot = m->index[snum].slot,
      .map = m,
    },
  };

out:
  return (e_sdrec_t) {
    .err = err,
    .sdrec = {
      .id = SRID_NIL,
      .size = 0,
      .flag = 0,
      .slot = NULL,
      .map = NULL,
    },
  };
}

/*************************************************************************************************/

inline static
err_r * srid_insert(trie_t * t, srid_t id, void * p, bool rep)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  uint64_t value = (uint64_t) p;
  return trie_insert(t, sizeof(key.b), key.b, value, rep);
}

inline static
void srid_delete(trie_t * t, srid_t id)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  if (trie_delete(t, sizeof(key.b), key.b)) {
    err_reset(); // should never happen...
    assert(0);
  }
}

inline static
e_void_t srid_find(trie_t * t, srid_t id)
{
  union {
    srid_t  id;
    uint8_t b[sizeof(srid_t)];
  } key = {.id = id};
  e_uint64_t e = trie_find(t, sizeof(key.b), key.b);
  if (e.err) {
    err_reset();
    return (e_void_t) {err_return(ERR_FAILURE, "id not found"), NULL};
  }
   return (e_void_t) {NULL, (void *) e.uint64};
}

/*************************************************************************************************/

static
err_r * sclass_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  store_t  * s = (store_t *) g;
  sclass_t * c = (sclass_t *) o;

  if (argc != 4) {
    return err_return(ERR_IN_INVALID, "not enough arguments to initialize sclass");
  }

  c->id = va_arg(ap, srid_t);
  c->meta = va_arg(ap, smrec_t *);
  c->fcnt = va_arg(ap, int);

  uint8_t * p = va_arg(ap, uint8_t *);

  for (uint16_t k = 0; k < c->fcnt; k++) {
    c->flds[k].kind = sdisk_u16_read(&p);
    srid_t mid = sdisk_u32_read(&p);
    if (mid != SRID_NIL) {
      e_smrec_t e = store_get_object(s, mid);
      if (e.err) {
        return err_return(ERR_CORRUPTION, "could not retreive referred field meta record");
      }
      c->flds[k].meta = e.smrec;
    } else {
      c->flds[k].meta = NULL;
    }
  }

  if (srid_insert(&s->i2r, c->id, c, 0)) {
    return err_return(ERR_FAILURE, "could not add id to live objects map");
  }

  e_spmap_t e = spman_load(&s->pm, SRID_TO_PAGE(c->id));
  if (e.err) {
    return err_return(ERR_CORRUPTION, "could not load page");
  }
  e.spmap->inuse++;

  return 0;
}

static
size_t sclass_clear(gc_global_t * g, gc_hdr_t * o)
{
  store_t  * s = (store_t *) g;
  sclass_t * c = (sclass_t *) o;

  srid_delete(&s->i2r, c->id);

  e_spmap_t e = spman_load(&s->pm, SRID_TO_PAGE(c->id));
  if (e.err) {
    err_reset();
    assert(0); /* should never happen */
  } else {
    e.spmap->inuse--;
  }

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

e_sclass_t store_get_class(store_t * s, srid_t id)
{
  e_sdrec_t e = spman_get(&s->pm, id);
  if (e.err) {
    return (e_sclass_t) {err_return(ERR_IN_INVALID, "could not retreive slot"), NULL};
  }

  if ((e.sdrec.flag & SSLOT_USAGEMASK) != SSLOT_USAGE_CLASS) {
    return (e_sclass_t) {err_return(ERR_IN_INVALID, "slot s not a class"), NULL};
  }

  e_void_t ee = srid_find(&s->i2r, e.sdrec.id);
  if (ee.err) {
    err_reset();

    uint8_t * p = e.sdrec.slot;
    smrec_t * meta = NULL;

    srid_t mid = sdisk_u32_read(&p);
    uint16_t fcnt = sdisk_u16_read(&p);

    if (mid != SRID_NIL) {
      e_smrec_t eee = store_get_object(s, mid);
      if (eee.err) {
        return (e_sclass_t) {err_return(ERR_CORRUPTION, "could not retreive referred class meta record"), NULL};
      }
      meta = eee.smrec;
    }
    spman_ref(&s->pm, e.sdrec.map);
    ee = gc_new(&s->g, &sclass_vtable, sizeof(sclass_t) + sizeof(scfld_t) * fcnt,
                     4, id, meta, fcnt, p);
    if (ee.err) {
      spman_unref(&s->pm, e.sdrec.map);
      return (e_sclass_t) {err_return(ERR_CORRUPTION, "could not unpack class"), NULL};
    }
    spman_unref(&s->pm, e.sdrec.map);
  }

  return (e_sclass_t) {NULL, ee.value};
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

static
err_r * smrec_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  store_t * s = (store_t *) g;
  smrec_t * r = (smrec_t *) o;
  err_r * err = NULL;

  if (argc != 3) {
    return err_return(ERR_IN_INVALID, "not enough arguments to initialize record");
  }

  r->sc = va_arg(ap, sclass_t *);
  r->id = va_arg(ap, srid_t);

  r->sz = sclass_mem_size(r->sc);

  uint8_t * slot = va_arg(ap, uint8_t *);

  r->ptr = malloc(r->sz);
  if (!r->ptr) {
    return err_return(ERR_MEM_ALLOC, "could not allocate memory to unpack");
  }

#if VERBOSEDEBUG
  fprintf(stderr, "{M} object <"PTR_FMT":"SRID_FMT"> of class <"PTR_FMT":"CLASS_FMT">\n",
    (void *) r, r->id, (void *) r->sc, r->sc->id);
#endif

  uint8_t * sp = slot;
  uint8_t * dp = r->ptr;
  for (uint16_t k = 0; k < r->sc->fcnt; k++) {
    switch (r->sc->flds[k].kind) {
      case SKIND_INT32:
        dp = smem_i32_write(dp, sdisk_i32_read(&sp)); break;
      case SKIND_UINT32:
        dp = smem_u32_write(dp, sdisk_u32_read(&sp)); break;
      case SKIND_INT64:
        dp = smem_i64_write(dp, sdisk_i64_read(&sp)); break;
      case SKIND_UINT64:
        dp = smem_u64_write(dp, sdisk_u64_read(&sp)); break;
      case SKIND_DOUBLE:
        dp = smem_dbl_write(dp, sdisk_dbl_read(&sp)); break;
      case SKIND_STRING: {
        uint16_t l = sdisk_u16_read(&sp);
        e_gc_str_t e = gc_new_str(&s->g, l, (char *) sp);
        if (e.err) {
          err = err_return(ERR_FAILURE, "could not allocate unpacked string"); goto out;
        }
        sp += ALIGN2(l);
        dp = smem_ptr_write(dp, e.gc_str);
#if VERBOSEDEBUG
        fprintf(stderr, "    element of <%p:%u> (string) <%.*s>\n",
          (void *) r, r->id, gc_str_len(e.gc_str), e.gc_str->data);
#endif
        break;
      }
      case SKIND_OBJECT: {
        srid_t rid = sdisk_u32_read(&sp);
        if (rid != SRID_NIL) {
          e_smrec_t e = store_get_object(s, rid);
          if (e.err) {
            err = err_return(ERR_FAILURE, "could not retreive referred record"); goto out;
          }
          dp = smem_ptr_write(dp, e.smrec);
#if VERBOSEDEBUG
          fprintf(stderr, "    element of <%p:%u> (object) <"PTR_FMT":"SRID_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) e.smrec, e.smrec->id, e.smrec->sc->id);
#endif
        } else {
          dp = smem_ptr_write(dp, NULL);
#if VERBOSEDEBUG
          fprintf(stderr, "    element of <%p:%u> (object) <"PTR_FMT">\n",
            (void *) r, r->id, NULL);
#endif
        }
        break;
      }
      case SKIND_CLASS: {
        srid_t cid = sdisk_u32_read(&sp);
        if (cid != SRID_NIL) {
          e_sclass_t e = store_get_class(s, cid);
          if (e.err) {
            err = err_return(ERR_FAILURE, "could not retreive referred class"); goto out;
          }
          dp = smem_ptr_write(dp, e.sclass);
#if VERBOSEDEBUG
          fprintf(stderr, "    element of <%p:%u> (class) <"PTR_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) e.sclass, e.sclass->id);
#endif
        } else {
          dp = smem_ptr_write(dp, NULL);
#if VERBOSEDEBUG
          fprintf(stderr, "    element of <%p:%u> (class) <"PTR_FMT">\n",
            (void *) r, r->id, NULL);
#endif
        }
        break;
      }
    }
  }

  if(srid_insert(&s->i2r, r->id, r, 0)) {
    err = err_return(ERR_FAILURE, "could not add id to live objects map"); goto out;
  }

  return NULL;

out:
  free(r->ptr);
  return err;
}

static
size_t smrec_clear(gc_global_t * g, gc_hdr_t * o)
{
  store_t * s = (store_t *) g;
  smrec_t * r = (smrec_t *) o;

  srid_delete(&s->i2r, r->id);

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

e_smrec_t store_get_object(store_t * s, srid_t id)
{
  e_sdrec_t e = spman_get(&s->pm, id);
  if (e.err) {
    return (e_smrec_t) {err_return(ERR_IN_INVALID, "could not retreive slot"), NULL};
  }

  if ((e.sdrec.flag & SSLOT_USAGEMASK) != SSLOT_USAGE_OBJECT) {
    return (e_smrec_t) {err_return(ERR_IN_INVALID, "slot s not a record"), NULL};
  }

  e_void_t ee = srid_find(&s->i2r, e.sdrec.id);
  if (ee.err) {
    err_reset();

    e_sclass_t eee = store_get_class(s, *((srid_t *) e.sdrec.slot));
    if (eee.err) {
      return (e_smrec_t) {err_return(ERR_CORRUPTION, "could not retreive record class"), NULL};
    }

    /* pin the page (map) so it does not vanish in the heat of the moment */
    spman_ref(&s->pm, e.sdrec.map);
    ee = gc_new(&s->g, &smrec_vtable, sizeof(smrec_t), 3, eee.sclass, id, e.sdrec.slot + sizeof(srid_t));
    if (ee.err) {
      spman_unref(&s->pm, e.sdrec.map);
      return (e_smrec_t) {err_return(ERR_FAILURE, "could not unpack record"), NULL};
    }
    spman_unref(&s->pm, e.sdrec.map);
  }

  return (e_smrec_t) {NULL, ee.value};
}

/*************************************************************************************************/

#define HEADER_STR0 "STORE000"

err_r * store_init(store_t * s, const char path[])
{
  if (!path) {
    return err_return(ERR_IN_INVALID, "no path to store given");
  }

  s->hdr = NULL;

  int fd = open(path, O_RDWR);
  if (fd == -1) {
    fd = open(path, O_CREAT | O_RDWR, S_IRWXU);
    if (fd == -1) {
      return err_return(ERR_FAILURE, "could not create store file");
    }
    if (ftruncate(fd, 4096)) {
      close(fd);
      unlink(path);
      return err_return(ERR_FAILURE, "could not create store header");
    }
    s->hdr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s->hdr == MAP_FAILED) {
      close(fd);
      unlink(path);
      return err_return(ERR_FAILURE, "could not map store header");
    }
    memset(s->hdr, 0xff, 4096);
    memcpy(s->hdr, HEADER_STR0, 8);
    sdisk_u32_write(s->hdr + 8, 0);
  }

  if (!s->hdr) {
    s->hdr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (s->hdr == MAP_FAILED) {
      close(fd);
      return err_return(ERR_FAILURE, "could not map store header");
    }
  }

  if (memcmp(s->hdr, HEADER_STR0, 8) != 0) {
    munmap(s->hdr, 4096);
    close(fd);
    s->hdr = NULL;
    return err_return(ERR_FAILURE, "file is not a store");
  }

  uint8_t * h = s->hdr + 8;
  if (spman_init(&s->pm, fd, 4096, sdisk_u32_read(&h))) {
    munmap(s->hdr, 4096);
    close(fd);
    return err_return(ERR_FAILURE, "could not initialize page manager");
  }

  if (gc_init(&s->g)) {
    spman_clear(&s->pm);
    munmap(s->hdr, 4096);
    close(fd);
    return err_return(ERR_FAILURE, "could not initialize garbage collector");
  }

  if (trie_init(&s->i2r, 8)) {
    gc_clear(&s->g);
    spman_clear(&s->pm);
    munmap(s->hdr, 4096);
    close(fd);
    return err_return(ERR_FAILURE, "could not initialize trie");
  }

  s->limbs = NULL;

  return NULL;
}

void store_clear(store_t * s)
{
  gc_clear(&s->g);
  trie_clear(&s->i2r);
  s->limbs = NULL;

  if (s->hdr) {
    sdisk_u32_write(s->hdr + 8, s->pm.cnt);
    munmap(s->hdr, 4096);
  }

  int fd = spman_clear(&s->pm);
  if (fd != -1) {
    close(fd);
  }
}

e_sclass_t store_add_class(store_t * s, smrec_t * meta, uint16_t fcnt, scfld_t flds[fcnt])
{
  uint32_t sz = sizeof(srid_t) + sizeof(uint16_t) + fcnt * (sizeof(uint16_t) + sizeof(srid_t));
  if (sz >= STORE_DATAMAX) {
    return (e_sclass_t) {err_return(ERR_OVERFLOW, "attempted to create a class lager than DATASIZE"), NULL};
  }

  e_sdrec_t e = spman_add(&s->pm, sz, SSLOT_USAGE_CLASS);
  if (e.err) {
    return (e_sclass_t) {err_return(ERR_MEM_ALLOC, "could not add new slot"), NULL};
  }

  uint8_t * p = e.sdrec.slot;

#if VERBOSEDEBUG
  fprintf(stderr, "[D] writing class [1;35m%u[0;m of %u elements {%u bytes}\n",
    e.sdrec.id, fcnt, sz);
#endif

  p = sdisk_obj_write(p, meta);
  p = sdisk_u16_write(p, fcnt);

  for (uint16_t k = 0; k < fcnt; k++) {
    p = sdisk_u16_write(p, flds[k].kind);
    p = sdisk_obj_write(p, flds[k].meta);
  }

  return store_get_class(s, e.sdrec.id);
}

e_smrec_t store_add_object(store_t * s, sclass_t * c, ...)
{
  va_list ap;

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

  if (sz >= STORE_DATAMAX) {
    return (e_smrec_t) {err_return(ERR_OVERFLOW, "attempted to create a record lager than DATASIZE"), NULL};
  }

  e_sdrec_t e = spman_add(&s->pm, sz, SSLOT_USAGE_OBJECT);
  if (e.err) {
    return (e_smrec_t) {err_return(ERR_MEM_ALLOC, "could not add new slot"), NULL};
  }

  uint8_t * p = e.sdrec.slot;

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

  return store_get_object(s, e.sdrec.id);
}
