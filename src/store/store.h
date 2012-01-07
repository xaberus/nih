#include "common/err.h"
#include "trie/trie.h"
#include "gc/gc.h"
#include "gc/gc-stack.h"

/*
 * 64kiB pages with maximal 4096 elements per page, i.e 4 dwords per element
 * tracking each page will cost at least 2^20 B = 1 MiB...
 */

typedef uint32_t srid_t;
#define SRID_NIL ~((srid_t) 0)

/* namespace partition in small pages */
#define STORE_SLOTBITS 12
#define STORE_PAGEBITS (sizeof(srid_t) * 8 - STORE_SLOTBITS)

/* data size must fit into uint16_t */
#define STORE_DATABITS 16
#define STORE_DATASIZE (1 << STORE_DATABITS)

/* page size must be multiple of this */
#define STORE_DATACHUNK 4096

#define STORE_PAGEMASK (~((srid_t) 0) << STORE_SLOTBITS)
#define STORE_SLOTMASK (~((srid_t) 0) >> STORE_PAGEBITS)

/* maximal slots per page */
#define STORE_SLOTSMAX (1 << STORE_SLOTBITS)

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
 *     u8[ALIGN16(size)]@data
 * )[count]
 */

typedef struct spmap spmap_t;
struct spmap {
  uint32_t   pnum;    /* page number */
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
};

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

spman_t * spman_init(spman_t * pm, int fd, off_t offset, uint32_t cnt);
int       spman_clear(spman_t * pm);

spmap_t * spman_load(spman_t * pm, uint32_t pnum);
spmap_t * spman_ref(spman_t * pm, spmap_t * m);
void      spman_unref(spman_t * pm, spmap_t * m);

sdrec_t   spman_add(spman_t * pm, uint16_t size, uint16_t usage);
sdrec_t   spman_get(spman_t * pm, srid_t id);


typedef enum skind {
  SKIND_INT32,
  SKIND_INT64,
  SKIND_UINT32,
  SKIND_UINT64,
  SKIND_DOUBLE,
  SKIND_STRING,
  SKIND_OBJECT,
  SKIND_CLASS,
} skind_t;

typedef struct sclass {
  gc_hdr_t gch;
  srid_t   id;     /* class record id */
  uint16_t cnt;    /* number of fields */
  skind_t  kind[];
} sclass_t;

typedef struct smrec smrec_t;
struct smrec {
  gc_obj_t   gco;
  sclass_t * sc;    /* pointer unpacked class this data implements */
  srid_t     id;    /* data record id */
  void     * ptr;   /* pointer to unpacked data */
  smrec_t  * limb;  /* pointer to next data record in chain */
  uint16_t   sz;    /* size of unpacked data */
};

typedef struct sfile {
  uint8_t  str0[8];
  uint32_t cnt;
} sfile_t;

typedef struct store {
  gc_global_t  g;      /* gc must go first for upcasts to work! */
  spman_t      pm;     /* page manager */
  gc_stack_t * limbs;  /* allocated array of limbs */
  trie_t       i2r;    /* map of ids to corresponding unpacked representation */
  sfile_t    * hdr;    /* mapped header of store */
} store_t;


store_t  * store_init(store_t * s, const char path[]);
void       store_clear(store_t * s);

/* class slot:  (<cnt> <kind 0> <kind 1> ... <kind (count-1)>) */
sclass_t * store_add_class(store_t * s, uint16_t cnt, skind_t kindv[cnt]);
sclass_t * store_get_class(store_t * s, srid_t id);

/* object slot: (<class> <element@kind 0> <element@kind 1> ... <element@kind (count-1)>) */
smrec_t  * store_add_object(store_t * s, sclass_t * c, ...);
smrec_t  * store_get_object(store_t * s, srid_t id);
