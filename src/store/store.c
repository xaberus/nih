#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stddef.h>

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

#define VERBOSEDEBUG 1

#define CLASS_FMT "[1;35m%u[0;m"
#define PTR_FMT "[1;33m%p[0;m"
#define PAGE_FMT "[1;32m%u[0;m"
#define SRID_FMT "[1;31m%u[0;m"
#define INT32_FMT "%d"
#define UINT32_FMT "%u"
#define INT64_FMT "%ld"
#define UINT64_FMT "%lu"

#define ELEMENT(_t, _p) (*((_t *) (_p)))

#include "store/sdisk.h"


inline static
uint16_t smem_size(skind_t kind)
{
  switch (kind) {
    case SKIND_NONE: return 0;
		case SKIND_UINT8: return sizeof(uint8_t); break;
		case SKIND_UINT16: return sizeof(uint16_t); break;
    case SKIND_INT32: return sizeof(int32_t); break;
    case SKIND_UINT32: return sizeof(uint32_t); break;
    case SKIND_INT64: return sizeof(int64_t); break;
    case SKIND_UINT64: return sizeof(uint64_t); break;
    case SKIND_DOUBLE: return sizeof(double); break;
    case SKIND_STRING: return sizeof(gc_str_t *); break;
    case SKIND_OBJECT: return sizeof(smrec_t *); break;
    case SKIND_ODREF: return sizeof(soref_t); break;
    case SKIND_CLASS: return sizeof(sclass_t *); break;
  }
  return 0;
}

inline static
uint16_t smem_align(uint16_t offset, skind_t kind)
{
	struct au8 {uint8_t a; uint8_t val;};
	struct au16 {uint8_t a; uint16_t val;};
  struct ai32 {uint8_t a; int32_t val;};
  struct au32 {uint8_t a; uint32_t val;};
  struct ai64 {uint8_t a; int64_t val;};
  struct au64 {uint8_t a; uint64_t val;};
  struct adbl {uint8_t a; double val;};
  struct aptr {uint8_t a; void * val;};
  struct aref {uint8_t a; soref_t val;};
  uint16_t pos = 0;
  switch (kind) {
    case SKIND_NONE: pos = 0; break;
		case SKIND_UINT8: pos = offsetof(struct au8, val); break;
		case SKIND_UINT16: pos = offsetof(struct au16, val); break;
    case SKIND_INT32: pos = offsetof(struct ai32, val); break;
    case SKIND_UINT32: pos = offsetof(struct au32, val); break;
    case SKIND_INT64: pos = offsetof(struct ai64, val); break;
    case SKIND_UINT64: pos = offsetof(struct au64, val); break;
    case SKIND_DOUBLE: pos = offsetof(struct adbl, val); break;
    case SKIND_STRING: pos = offsetof(struct aptr, val); break;
    case SKIND_OBJECT: pos = offsetof(struct aptr, val); break;
    case SKIND_ODREF: pos = offsetof(struct aref, val); break;
    case SKIND_CLASS: pos = offsetof(struct aptr, val); break;
  }
  if (pos) {
    return (((offset) + (pos-1)) & ~(pos-1));
  }
  return offset;
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
    // fprintf(stdout, "IDX %08u|%04u|%04u|%04u|%p\n", p - m->page, sid, sflag, ALIGN2(ssize), slot);
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
  uint16_t sflag;
  if (sid < STORE_SLOTSMAX) {
    if (!sid) {
      off = SPAGE_HDRSIZE;
    } else {
      off = (uint8_t *) m->index[sid - 1].slot - (uint8_t *) m->page;
      off += ALIGN2(m->index[sid - 1].size);
    }
    uint32_t needed = sz + SSLOT_HDRSIZE;
    uint32_t npsize = off + needed;
    //printf("TRY for sid %u {%u %u}\n", sid, needed, m->psize);
    if (npsize > m->psize) {
      /* check for overflow */
      if (npsize > m->psize) {
        if (spman_truncate(pm, m, npsize)) {
          err_reset();
        } else {
          if (off + sz + SSLOT_HDRSIZE <= m->psize) {
            goto do_alloc;
          }
        }
      }
    }
    goto do_alloc;
  }
  return (sdrec_t) {
    .id = SRID_NIL,
    .size = 0,
    .flag = 0,
    .slot = NULL,
    .map = NULL,
  };
do_alloc:
  p = (uint8_t *) m->page + off;
  sflag = SSLOT_FLAG_DATA | (usage & SSLOT_USAGEMASK);
  p = sdisk_u16_write(p, sflag);
  p = sdisk_u16_write(p, ssize);
  // fprintf(stdout, "OFF %08u|%04u|%04u|%04u\n", off, sid, sflag| ssize);
  sslot_t * s = &m->index[sid];
  s->flag = sflag;
  s->size = ssize;
  s->slot = p;
  m->scount++;
  uint8_t * wp = m->page;
  sdisk_u16_read(&wp);
  sdisk_u16_write(wp, m->scount);
#if VERBOSEDEBUG > 2
  printf("{#} page "PAGE_FMT" : (%u/%u slot)\n",  m->pnum, m->scount, STORE_SLOTSMAX);
#endif
  return (sdrec_t) {
    .id = (m->pnum << STORE_SLOTBITS) | sid,
    .size = ssize,
    .flag = sflag,
    .slot = p,
    .map = m,
  };
}

spmap_t * spman_pget(spman_t * pm, uint32_t pnum)
{
  if (pnum < pm->cnt) {
    uint32_t  hash = hash32(pnum) & STORE_LIVEMASK;
    spmap_t * c = pm->map[hash];
    while (c) {
      if (c->pnum == pnum) {
        return c;
      }
      c = c->next;
    }
  }
  return NULL;
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
    uint32_t maxc = 0;
    for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
      spmap_t * n = pm->map[k];
      while (n) {
        if (n->live && n->ref == 1) {
          maxc = maxc < n->inuse ? n->inuse : maxc;
        }
        n = n->next;
      }
    }
#if VERBOSEDEBUG > 2
    fprintf(stdout, "[P] going to drop one of: (c: %u)\n", maxc);
    for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
      spmap_t * n = pm->map[k];
      while (n) {
        fprintf(stdout, "      page "PTR_FMT":"PAGE_FMT" (%u/%u slots) {%u bytes} [%u, %u]\n",
          (void *) n->page, n->pnum, n->scount, STORE_SLOTSMAX, n->psize, n->inuse, n->ref);
        n = n->next;
      }
    }
#endif
    spmap_t * del = NULL;
    for (uint32_t k = 0; k < STORE_LIVEPAGES; k++) {
      spmap_t * n = pm->map[k];
      while (n) {
        if (n->live && n->ref == 1) {
          if (!del && n->inuse == maxc) {
            del = n;
          } else {
            uint32_t inuse = n->inuse + n->scount;
            if (inuse > n->inuse) {
              n->inuse = inuse;
            }
          }
        }
        n = n->next;
      }
    }
    if (del) {
#if VERBOSEDEBUG > 2
    //if (del->scount < STORE_SLOTSMAX) {
      fprintf(stdout, "[P] dropping page "PTR_FMT":"PAGE_FMT" (%u/%u slots) {%u bytes} of:\n",
        (void *) del->page, del->pnum, del->scount, STORE_SLOTSMAX, del->psize);
    //}
#endif
      del->live = 0;
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
  m->live = 1;
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
  fprintf(stdout, "[P] loaded page "PTR_FMT":"PAGE_FMT" (%u/%u slots) {%u bytes}\n",
    (void *) m->page, pnum, scount, STORE_SLOTSMAX, psize);
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
    uint8_t * p = m->page;
    sdisk_u16_read(&p);
    sdisk_u16_write(p, m->scount);
#if VERBOSEDEBUG > 0
    fprintf(stdout, "{P} unloading page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes}\n",
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
#if VERBOSEDEBUG > 3
    fprintf(stdout, "{P} +++  refc page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes} [%p@%u]\n",
      (void *) m->page, m->pnum, m->scount, m->psize, (void *) m, m->ref);
#endif
  (void) pm;
  m->ref++;
  return m;
}

void spman_unref(spman_t * pm, spmap_t * m)
{
#if VERBOSEDEBUG > 3
    fprintf(stdout, "{P} --- urefc page "PTR_FMT":"PAGE_FMT" (%u slots) {%u bytes} [%p@%u]\n",
      (void *) m->page, m->pnum, m->scount, m->psize, (void *) m, m->ref);
#endif
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
      fprintf(stdout, "[P] resizing store to %u pages (anum was "PAGE_FMT")\n",
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
  fprintf(stdout, "[P] appended %u bytes to store\n", psize);
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
  if (c->meta) {
    gc_barrier(&s->g, GC_OBJ(s), GC_HDR(c->meta));
  }
  c->fcnt = va_arg(ap, int);

  uint8_t * p = va_arg(ap, uint8_t *);

  uint16_t offset = 0;

  /* register cid here to make recursive definitions work */
  if (srid_insert(&s->i2r, c->id, c, 0)) {
    return err_return(ERR_FAILURE, "could not add id to live objects map");
  }

  for (uint16_t k = 0; k < c->fcnt; k++) {
    scfld_t * fr = &c->flds[k];
    fr->kind = sdisk_u16_read(&p);
    offset = smem_align(offset, fr->kind);
    fr->offset = offset;
    offset += smem_size(fr->kind);
    srid_t cid = sdisk_rid_read(&p);
    if (cid != SRID_NIL) {
      e_sclass_t e = store_get_class(s, cid);
      if (e.err) {
        return err_return(ERR_CORRUPTION, "could not retreive referred field class record");
      }
      fr->cls = e.sclass; /* nobarrier: init */
    } else {
      fr->cls = NULL;
    }
    srid_t mid = sdisk_rid_read(&p);
    if (mid != SRID_NIL) {
      e_smrec_t e = store_get_object(s, mid);
      if (e.err) {
        return err_return(ERR_CORRUPTION, "could not retreive referred field meta record");
      }
      fr->meta = e.smrec;
    } else {
      fr->meta = NULL;
    }
  }

  spmap_t * m = spman_pget(&s->pm, SRID_TO_PAGE(c->id));
  if (m) {
    m->inuse++;
  }

#if VERBOSEDEBUG
  fprintf(stdout, "{M} class <"PTR_FMT":"CLASS_FMT">\n", (void *) c, c->id);
#endif

  return 0;
}

static
size_t sclass_clear(gc_global_t * g, gc_hdr_t * o)
{
  store_t  * s = (store_t *) g;
  sclass_t * c = (sclass_t *) o;

#if VERBOSEDEBUG > 1
  fprintf(stdout, "<M> deleting class <"PTR_FMT":"CLASS_FMT">\n", (void *) c, c->id);
#endif

  srid_delete(&s->i2r, c->id);

  spmap_t * m = spman_pget(&s->pm, SRID_TO_PAGE(c->id));
  if (m) {
    m->inuse--;
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
  .name = "sclass_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = sclass_init,
  .gc_clear = sclass_clear,
  .gc_propagate = sclass_propagate,
};

e_sclass_t store_get_class(store_t * s, srid_t id)
{
  e_void_t ee = srid_find(&s->i2r, id);
  if (ee.err) {
    err_reset();

    e_sdrec_t e = spman_get(&s->pm, id);
    if (e.err) {
      return (e_sclass_t) {err_return(ERR_IN_INVALID, "could not retreive slot"), NULL};
    }

    if ((e.sdrec.flag & SSLOT_USAGEMASK) != SSLOT_USAGE_CLASS) {
      return (e_sclass_t) {err_return(ERR_IN_INVALID, "slot s not a class"), NULL};
    }

    uint8_t * p = e.sdrec.slot;
    smrec_t * meta = NULL;

    srid_t mid = sdisk_rid_read(&p);
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
    switch ((skind_t) c->flds[k].kind) {
      case SKIND_NONE: break;
			case SKIND_UINT8: r++; break;
			case SKIND_UINT16: r++; break;
      case SKIND_INT32: r++; break;
      case SKIND_UINT32: r++; break;
      case SKIND_INT64: r++; break;
      case SKIND_UINT64: r++; break;
      case SKIND_DOUBLE: r++; break;
      case SKIND_STRING: r += 2; break;
      case SKIND_OBJECT: r++; break;
      case SKIND_ODREF: r++; break;
      case SKIND_CLASS: r++; break;
    }
  }
  return r;
}

uint16_t sclass_mem_size(sclass_t * c)
{
  uint16_t sz = 0;
  for (uint16_t k = 0; k < c->fcnt; k++) {
    skind_t kind = c->flds[k].kind;
    sz = smem_align(sz, kind) + smem_size(kind);
  }
  return sz;
}

size_t sclass_walk_propagate(store_t * s, sclass_t * c, uint8_t * p)
{
  size_t n = 0;
  for (uint16_t k = 0; k < c->fcnt; k++) {
    switch ((skind_t) c->flds[k].kind) {
      case SKIND_NONE:
			case SKIND_UINT8:
			case SKIND_UINT16:
      case SKIND_INT32:
      case SKIND_UINT32:
      case SKIND_INT64:
      case SKIND_UINT64:
      case SKIND_DOUBLE:
        break;
      case SKIND_STRING: {
        gc_str_t * str = *(gc_str_t **) (p + c->flds[k].offset);
        gc_mark(&s->g, GC_HDR(str)); n++;
      } break;
      case SKIND_OBJECT: {
        smrec_t * rec = *(smrec_t **) (p + c->flds[k].offset);
        if (rec) {
          gc_mark(&s->g, GC_HDR(rec)); n++;
        }
      } break;
      case SKIND_ODREF: {
        soref_t * o = (soref_t *) (p + c->flds[k].offset);
        if (o->ref) {
          gc_mark(&s->g, GC_HDR(o->ref)); n++;
        }
      } break;
      case SKIND_CLASS: {
        sclass_t * cls = *(sclass_t **) (p + c->flds[k].offset);
        gc_mark(&s->g, GC_HDR(cls)); n++;
      } break;
    }
  }
  return n;
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

  r->sc = va_arg(ap, sclass_t *); /* nobarrier: init */
  r->id = va_arg(ap, srid_t);

  r->sz = sclass_mem_size(r->sc);

  uint8_t * slot = va_arg(ap, uint8_t *);

  r->ptr = malloc(r->sz);
  if (!r->ptr) {
    return err_return(ERR_MEM_ALLOC, "could not allocate memory to unpack");
  }

#if VERBOSEDEBUG
  fprintf(stdout, "{M} object <"PTR_FMT":"SRID_FMT"> of class <"PTR_FMT":"CLASS_FMT">\n",
    (void *) r, r->id, (void *) r->sc, r->sc->id);
#endif

  uint8_t * sp = slot;
  uint8_t * dptr = r->ptr;
  for (uint16_t k = 0; k < r->sc->fcnt; k++) {
    scfld_t * d = &r->sc->flds[k];
    uint8_t * dp = dptr + d->offset;
    switch ((skind_t) d->kind) {
      case SKIND_NONE:
        break;
			case SKIND_UINT8:
				ELEMENT(uint8_t, dp) = sdisk_u8_read(&sp); break;
			case SKIND_UINT16:
				ELEMENT(uint16_t, dp) = sdisk_u16_read(&sp); break;
      case SKIND_INT32:
        ELEMENT(int32_t, dp) = sdisk_i32_read(&sp); break;
      case SKIND_UINT32:
        ELEMENT(uint32_t, dp) = sdisk_u32_read(&sp); break;
      case SKIND_INT64:
        ELEMENT(int64_t, dp) = sdisk_i64_read(&sp); break;
      case SKIND_UINT64:
        ELEMENT(uint64_t, dp) = sdisk_u64_read(&sp); break;
      case SKIND_DOUBLE:
        ELEMENT(double, dp) = sdisk_dbl_read(&sp); break;
      case SKIND_STRING: {
        uint16_t l = sdisk_u16_read(&sp);
        e_gc_str_t e = gc_new_str(&s->g, l, (char *) sp);
        if (e.err) {
          err = err_return(ERR_FAILURE, "could not allocate unpacked string"); goto out;
        }
        sp += ALIGN2(l);
        ELEMENT(gc_str_t *, dp) = e.gc_str; /* nobarrier: init */
#if VERBOSEDEBUG
        fprintf(stdout, "    element of <%p:%u> (string) <%.*s>\n",
          (void *) r, r->id, gc_str_len(e.gc_str), e.gc_str->data);
#endif
        break;
      }
      case SKIND_OBJECT: {
        srid_t rid = sdisk_rid_read(&sp);
        if (rid != SRID_NIL) {
          e_smrec_t e = store_get_object(s, rid);
          if (e.err) {
            err = err_return(ERR_FAILURE, "could not retreive referred record"); goto out;
          }
          ELEMENT(smrec_t *, dp) = e.smrec; /* nobarrier: init */
#if VERBOSEDEBUG
          fprintf(stdout, "    element of <%p:%u> (object) <"PTR_FMT":"SRID_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) e.smrec, e.smrec->id, e.smrec->sc->id);
#endif
        } else {
          ELEMENT(smrec_t *, dp) = NULL;
#if VERBOSEDEBUG
          fprintf(stdout, "    element of <%p:%u> (object) <"PTR_FMT">\n",
            (void *) r, r->id, NULL);
#endif
        }
        break;
      }
      case SKIND_ODREF: {
        ELEMENT(soref_t, dp) = (soref_t) {sdisk_rid_read(&sp), NULL};
      } break;
      case SKIND_CLASS: {
        srid_t cid = sdisk_rid_read(&sp);
        if (cid != SRID_NIL) {
          e_sclass_t e = store_get_class(s, cid);
          if (e.err) {
            err = err_return(ERR_FAILURE, "could not retreive referred class"); goto out;
          }
          ELEMENT(sclass_t *, dp) = e.sclass; /* nobarrier: init */
#if VERBOSEDEBUG
          fprintf(stdout, "    element of <%p:%u> (class) <"PTR_FMT":"CLASS_FMT">\n",
            (void *) r, r->id, (void *) e.sclass, e.sclass->id);
#endif
        } else {
          ELEMENT(sclass_t *, dp) = NULL;
#if VERBOSEDEBUG
          fprintf(stdout, "    element of <%p:%u> (class) <"PTR_FMT">\n",
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

  spmap_t * m = spman_pget(&s->pm, SRID_TO_PAGE(r->id));
  if (m) {
    m->inuse++;
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

#if VERBOSEDEBUG > 1
  fprintf(stdout, "<M> deleting object <"PTR_FMT":"SRID_FMT">\n", (void *) r, r->id);
#endif

  spmap_t * m = spman_pget(&s->pm, SRID_TO_PAGE(r->id));
  if (m) {
    m->inuse--;
  }

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
  if (r->sc) {
    gc_mark(g, GC_HDR(r->sc));
  }
  sclass_walk_propagate(s, r->sc, r->ptr);
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
  e_void_t ee = srid_find(&s->i2r, id);
  if (ee.err) {
    err_reset();

    e_sdrec_t e = spman_get(&s->pm, id);
    if (e.err) {
      return (e_smrec_t) {err_return(ERR_IN_INVALID, "could not retreive slot"), NULL};
    }

    if ((e.sdrec.flag & SSLOT_USAGEMASK) != SSLOT_USAGE_OBJECT) {
      return (e_smrec_t) {err_return(ERR_IN_INVALID, "slot s not a record"), NULL};
    }

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

  return NULL;
}

void store_clear(store_t * s)
{
  gc_clear(&s->g);
  trie_clear(&s->i2r);

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
  // TODO: on-disk format
  uint32_t sz = sizeof(srid_t) + sizeof(uint16_t) +
    fcnt * (sizeof(uint16_t) + sizeof(srid_t) + sizeof(srid_t));
  if (sz >= STORE_DATAMAX) {
    return (e_sclass_t) {err_return(ERR_OVERFLOW, "attempted to create a class lager than DATASIZE"), NULL};
  }

  e_sdrec_t e = spman_add(&s->pm, sz, SSLOT_USAGE_CLASS);
  if (e.err) {
    return (e_sclass_t) {err_return(ERR_MEM_ALLOC, "could not add new slot"), NULL};
  }

  uint8_t * p = e.sdrec.slot;

#if VERBOSEDEBUG > 1
  fprintf(stdout, "[D] writing class [1;35m%u[0;m of %u elements {%u bytes}\n",
    e.sdrec.id, fcnt, sz);
#endif

  p = sdisk_obj_write(p, meta);
  p = sdisk_u16_write(p, fcnt);

  for (uint16_t k = 0; k < fcnt; k++) {
    p = sdisk_u16_write(p, flds[k].kind);
    p = sdisk_cls_write(p, flds[k].cls);
    p = sdisk_obj_write(p, flds[k].meta);
  }

  return store_get_class(s, e.sdrec.id);
}

e_smrec_t store_add_object(store_t * s, sclass_t * c, ...)
{
  va_list ap;

  uint32_t sz = sizeof(srid_t);

  union {
		uint8_t      u8;
    uint16_t     u16;
    int32_t      i32;
    uint32_t     u32;
    int64_t      i64;
    uint64_t     u64;
    srid_t       rid;
    double       dbl;
    const char * str;
    smrec_t    * obj;
    sclass_t   * cls;
  } args[sclass_instargc(c)];

  va_start(ap, c);
  for (uint16_t k = 0, v = 0; k < c->fcnt; k++) {
    switch ((skind_t) c->flds[k].kind) {
      case SKIND_NONE:
        break;
      case SKIND_UINT8:
        args[v++].u8 = va_arg(ap, int); sz += sdisk_size(SKIND_UINT8); break;
      case SKIND_UINT16:
        args[v++].u16 = va_arg(ap, int); sz += sdisk_size(SKIND_UINT16); break;
      case SKIND_INT32:
        args[v++].i32 = va_arg(ap, int32_t); sz += sdisk_size(SKIND_INT32); break;
      case SKIND_UINT32:
        args[v++].u32 = va_arg(ap, uint32_t); sz += sdisk_size(SKIND_UINT32); break;
      case SKIND_INT64:
        args[v++].i64 = va_arg(ap, int64_t); sz += sdisk_size(SKIND_INT64); break;
      case SKIND_UINT64:
        args[v++].u64 = va_arg(ap, uint64_t); sz += sdisk_size(SKIND_UINT64); break;
      case SKIND_DOUBLE:
        args[v++].dbl = va_arg(ap, double); sz += sdisk_size(SKIND_DOUBLE); break;
      case SKIND_STRING: {
        uint16_t l = va_arg(ap, int);
        const char * a = va_arg(ap, const char *);
        args[v++].u16 = l; sz += sdisk_size(SKIND_STRING);
        args[v++].str = a; sz += (a ? ALIGN2(l) : 0);
        break;
      }
      case SKIND_OBJECT:
        args[v++].obj = va_arg(ap, smrec_t *); sz += sdisk_size(SKIND_OBJECT); break;
      case SKIND_ODREF:
        args[v++].rid = va_arg(ap, srid_t); sz += sdisk_size(SKIND_ODREF); break;
      case SKIND_CLASS:
        args[v++].cls = va_arg(ap, sclass_t *); sz += sdisk_size(SKIND_CLASS); break;
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
  assert((e.sdrec.flag & SSLOT_USAGEMASK) == SSLOT_USAGE_OBJECT);

  uint8_t * p = e.sdrec.slot;

#if VERBOSEDEBUG > 1
  fprintf(stdout, "{D} writing object "SRID_FMT" of class "CLASS_FMT" with {%u elements, %u bytes}\n",
    e.sdrec.id, c->id, c->fcnt, sz);
#endif

  /* write class reference first */
  p = sdisk_cls_write(p, c);

  for (uint16_t k = 0, v = 0; k < c->fcnt; k++) {
    switch ((skind_t) c->flds[k].kind) {
      case SKIND_NONE:
        break;
			case SKIND_UINT8:
        p = sdisk_u8_write(p, args[v++].u8); break;
      case SKIND_UINT16:
        p = sdisk_u16_write(p, args[v++].u16); break;
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
      case SKIND_ODREF:
        p = sdisk_rid_write(p, args[v++].rid); break;
      case SKIND_CLASS: {
        p = sdisk_cls_write(p, args[v++].cls); break;
      }
    }
  }

  {
    e_smrec_t er = store_get_object(s, e.sdrec.id);
    if (er.err) {
      return (e_smrec_t) {err_return(ERR_FAILURE, "could not get object"), NULL};
    }
    return er;
  }
}

err_r * store_follow_ref(store_t * s, smrec_t * r, soref_t * o)
{
  if (!o->ref) {
    e_smrec_t e = store_get_object(s, o->rid);
    if (e.err) {
      return err_return(ERR_FAILURE, "could not get object");
    }
    o->ref = e.smrec;
    gc_barrier(&s->g, GC_OBJ(r), GC_HDR(e.smrec));
  }
  return NULL;
}
