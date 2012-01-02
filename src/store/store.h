#include "common/err.h"
#include "common/memory.h"
#include "trie/trie.h"
#include "gc/gc.h"
#include "gc/gc-stack.h"

/*
 * 64kiB pages with maximal 4096 elements per page, i.e 4 dwords per element
 * tracking each page will cost at least 2^20 B = 1 MiB...
 */

typedef uint32_t srid_t;
#define SRID_NIL ~((srid_t) 0)

#define STORE_SLOTBITS 12
#define STORE_DATABITS 16
#define STORE_PAGEBITS (sizeof(srid_t)*8 - STORE_SLOTBITS)
#define STORE_PAGEMASK (~((srid_t) 0) << STORE_SLOTBITS)
#define STORE_SLOTMASK (~((srid_t) 0) >> STORE_PAGEBITS)
#define STORE_PAGESIZE (1 << STORE_SLOTBITS)

/* memory page alignment! */
#define STORE_DATASIZE (1 << STORE_DATABITS)

#define STORE_LIVEPAGES 16
#define STORE_LIVEMASK  0xf

/* slot management flags, a slot is empty <=> (flag & FLAGMASK == 0) */
#define SSLOT_FLAG_DATA 0x0001
#define SSLOT_FLAG_MOVD 0x0002
#define SSLOT_FLAGMASK  0x000f

/* slot usage */
#define SSLOT_USAGE_NONE   0x0000
#define SSLOT_USAGE_CLASS  0x0010
#define SSLOT_USAGE_OBJECT 0x0020
#define SSLOT_USAGEMASK    0xfff0

typedef struct sslot {
  uint16_t flag;
  uint16_t size;
  uint16_t offset;
} sslot_t;

typedef struct spage {
  sslot_t  info[STORE_PAGESIZE];
  uint8_t  data[STORE_DATASIZE];
} spage_t;

typedef struct spmap spmap_t;
struct spmap {
  uint32_t  pnum;
  uint32_t  cnt;
  uint32_t  ref;
  spage_t * page;
  spmap_t * next;
};

typedef struct sdrec {
  srid_t    id;
  uint16_t  size;
  uint16_t  flag;
  uint8_t * slot;
  spmap_t * map;
} sdrec_t;

typedef struct spman {
  spmap_t * map[STORE_LIVEPAGES];
  uint16_t  maps;
  int       fd;
  off_t     offset;
  uint32_t  cnt;
  uint32_t  anum;
} spman_t;

spman_t * spman_init(gc_global_t * g, spman_t * pm, int fd, off_t offset, uint32_t cnt);
int       spman_clear(gc_global_t * g, spman_t * pm);

spmap_t * spman_load(gc_global_t * g, spman_t * pm, uint32_t pnum);
spmap_t * spman_ref(gc_global_t * g, spman_t * pm, spmap_t * m);
void      spman_unref(gc_global_t * g, spman_t * pm, spmap_t * m);

sdrec_t   spman_add(gc_global_t * g, spman_t * pm, uint16_t size, uint16_t usage);
sdrec_t   spman_get(gc_global_t * g, spman_t * pm, srid_t id);


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
  srid_t   id;
  uint16_t cnt;
  skind_t  kind[];
} sclass_t;

typedef struct smrec smrec_t;
struct smrec {
  gc_obj_t   gco;
  sclass_t * sc;
  srid_t     id;
  void     * ptr;
  smrec_t  * limb;
  uint16_t   sz;
};

typedef struct sfile {
  uint8_t  str0[8];
  uint32_t cnt;
} sfile_t;

typedef struct store {
  /* gc must go first for upcasts to work */
  gc_global_t  g;

  spman_t      pm;
  gc_stack_t * limbs;

  trie_t       i2r;

  sfile_t    * hdr;
} store_t;


store_t  * store_init(store_t * s, mema_t a, const char path[]);
void       store_clear(store_t * s);

/* class slot:  (<cnt> <kind 0> <kind 1> ... <kind (count-1)>) */
sclass_t * store_add_class(store_t * s, uint16_t cnt, skind_t kindv[cnt]);
sclass_t * store_get_class(store_t * s, srid_t id);

/* object slot: (<class> <element@kind 0> <element@kind 1> ... <element@kind (count-1)>) */
smrec_t  * store_add_object(store_t * s, sclass_t * c, ...);
smrec_t  * store_get_object(store_t * s, srid_t id);
