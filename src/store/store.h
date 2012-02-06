#ifndef _STORE_H
#define _STORE_H

#include "common/err.h"
#include "trie/trie.h"
#include "gc/gc.h"
#include "gc/gc-stack.h"

/*
 * 64kiB pages with maximal 4096 elements per page, i.e 4 dwords per element
 * tracking each page will cost at least 2^20 B = 1 MiB...
 */

typedef uint32_t srid_t;
#define SRID_NIL ((srid_t) -1)

/* namespace partition in small pages */
#define STORE_SLOTBITS 8 /*12*/
#define STORE_PAGEBITS (sizeof(srid_t) * 8 - STORE_SLOTBITS)

/* data size must fit into uint16_t */
#define STORE_DATAMAX ((1 << 16) - 1)

/* page size must be multiple of this */
//#define STORE_DATACHUNK 4096ul
#define STORE_DATACHUNK 4096u

/* allocate NEWPN data chunks for new pages */
#define STORE_NEWPN 1

#define STORE_PAGEMASK (~((srid_t) 0) << STORE_SLOTBITS)
#define STORE_SLOTMASK (~((srid_t) 0) >> STORE_PAGEBITS)

/* maximal slots per page */
#define STORE_SLOTSMAX ((1 << STORE_SLOTBITS))

/* maximal amount of pges to keep alive */
#define STORE_LIVEPAGES 16
#define STORE_LIVEMASK  0xf

/* slot management flags, a slot is empty <=> (flag & FLAGMASK == 0) */
#define SSLOT_FLAG_FLST 0x0001 /* first free slot */
#define SSLOT_FLAG_FREE 0x0002 /* free slot */
#define SSLOT_FLAG_DATA 0x0004 /* slot stores data */
#define SSLOT_FLAG_MOVD 0x0008 /* slot was moved */
#define SSLOT_FLAGMASK  0x000f

/* slot usage */
#define SSLOT_USAGE_NONE   0x0000
#define SSLOT_USAGE_CLASS  0x0010
#define SSLOT_USAGE_ARRAY  0x0030
#define SSLOT_USAGE_OBJECT 0x0020
#define SSLOT_USAGEMASK    0xfff0

typedef struct sslot {
  uint16_t flag;   /* metadata and usage flags */
  uint16_t size;   /* size of slot in bytes */
  void   * slot;   /* pointer to slot contents */
} sslot_t;

/* PAGE LAYOUT:
 *
 * u16@pn u16@count
 * (
 *   u16@flag u16@size
 *     u8[ALIGN2(size)]@data
 * )[count]
 */

typedef struct spmap spmap_t;
struct spmap {
  srid_t     pnum;    /* page number */
  uint32_t   inuse;   /* number of live objects from this page */
  uint32_t   ref;     /* references count */
  spmap_t ** rev;     /* pointer to pointer to this spmap */
  spmap_t *  next;    /* pointer to next spmap in hashmap */
  sslot_t    index[STORE_SLOTSMAX]; /* array with slot information */
  uint16_t   scount;  /* pointer to number of used slots in this page/index */
  uint32_t   psize;   /* 4096 * page@pn -- i.e size of page in bytes */
  void     * page;    /* page data with inlined slots an metadata */
  off_t      poff;    /* page offset in store */
  uint16_t   sfree;   /* 1 if there are free slots < scount */
  uint16_t   sfhdr;   /* first free slot if sfree == 1 */
  uint16_t   live;
};

/* volatile record data tuple */
typedef struct sdrec {
  srid_t    id;   /* record id */
  uint16_t  size; /* record size */
  uint16_t  flag; /* record flags */
  uint8_t * slot; /* pointer to record data */
  spmap_t * map;  /* pointer to page map */
} sdrec_t;

typedef struct spman {
  spmap_t * map[STORE_LIVEPAGES]; /* hashmap of live pages */
  uint16_t  maps;    /* number of live pages */
  int       fd;      /* store file descriptor */
  off_t     offset;  /* start of page manager chunk start in store */
  uint32_t  cnt;     /* number of pages in store */
  uint32_t  anum;    /* page to start seeking for free slots */
} spman_t;

err_r *   spman_init(spman_t * pm, int fd, off_t offset, uint32_t cnt);
int       spman_clear(spman_t * pm);

/* tuple */ typedef struct { err_r * err; spmap_t * spmap; } e_spmap_t;
e_spmap_t spman_load(spman_t * pm, srid_t pnum);
spmap_t * spman_ref(spman_t * pm, spmap_t * m);
void      spman_unref(spman_t * pm, spmap_t * m);
err_r   * spman_truncate(spman_t * pm, spmap_t * m, uint32_t psize);

/* tuple */ typedef struct { err_r * err; sdrec_t sdrec; } e_sdrec_t;
e_sdrec_t spman_add(spman_t * pm, uint16_t size, uint16_t usage);
e_sdrec_t spman_get(spman_t * pm, srid_t id);


typedef enum skind {
  SKIND_NONE = 0,
	SKIND_UINT8,
	SKIND_UINT16,
  SKIND_INT32,
  SKIND_UINT32,
  SKIND_INT64,
  SKIND_UINT64,
  SKIND_DOUBLE,
  SKIND_OBJECT,
  SKIND_ODREF,
  SKIND_CLASS,
} skind_t;

typedef struct smrec smrec_t;
typedef struct sclass sclass_t;

/* FIELD META LAYOUT: (u8[]@name) */

typedef struct scfld {
  skind_t    kind;    /* field kind */
  sclass_t * cls;
  smrec_t  * meta;    /* field meta or nil */
  uint16_t   offset;  /* field offset in unpacked record */
} scfld_t;

/* CLASS META LAYOUT: (u8[]@name) */

struct sclass {
  gc_obj_t  gch;
  srid_t    id;     /* class record id */
  smrec_t * meta;   /* class metadata (very likely just nil)*/
  uint16_t  fcnt;   /* number of fields */
  scfld_t   flds[];
};


struct smrec {
  gc_obj_t   gco;
  sclass_t * sc;    /* pointer unpacked class this data implements */
  srid_t     id;    /* data record id */
  void     * ptr;   /* pointer to unpacked data */
  uint16_t * otr;   /* pointer offset table in unpacked data (do not free!) */
  uint16_t   sz;    /* size of unpacked data */
};

typedef struct soref {
  srid_t    rid;
  smrec_t * ref;
} soref_t;

typedef struct store {
  gc_global_t  g;      /* gc must go first for upcasts to work! */
  spman_t      pm;     /* page manager */
  trie_t       i2r;    /* map of ids to corresponding unpacked representation */
  uint8_t    * hdr;    /* mapped header of store */
} store_t;

err_r * store_init(store_t * s, const char path[]);
void    store_clear(store_t * s);

/* CLASS SLOT:  rec@cmeta u16@fcnt (kind@fkind rec@fmeta)[fcnt] */
/* ARRAY SLOT:  rec@cmeta u16@fcnt (kind@fkind rec@fmeta)[1] */

/* tuple */ typedef struct { err_r * err; sclass_t * sclass; } e_sclass_t;
e_sclass_t store_add_class(store_t * s, smrec_t * meta, uint16_t fcnt, scfld_t flds[fcnt]);
e_sclass_t store_get_class(store_t * s, srid_t id);

typedef union {
  uint8_t    u8;
  uint16_t   u16;
  int32_t    i32;
  uint32_t   u32;
  int64_t    i64;
  uint64_t   u64;
  srid_t     rid;
  double     dbl;
  smrec_t  * obj;
  sclass_t * cls;
} svalue_t;

/* OBJECT SLOT: class@sc field<sc.fields[0].kind> ... field<sc.fields[sc.cnt].kind> */

/* field<xINTyy>: xyy@value */
/* field<OBJECT>: rec@value */
/* field<CLASS>: class@value */

/* tuple */ typedef struct { err_r * err; smrec_t * smrec; } e_smrec_t;
e_smrec_t store_add_object(store_t * s, sclass_t * c, ...);
e_smrec_t store_get_object(store_t * s, srid_t id);

err_r * store_follow_ref(store_t * s, smrec_t * r, soref_t * o);

#endif /* _STORE_H */