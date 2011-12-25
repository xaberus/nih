#include "trie/trie-private.h"

#define PDIR_ADDRBITS (32-PDIR_NODEBITS)
#define PDIR_ADDRMASK (0xffffffff << PDIR_NODEBITS)
#define PDIR_NODEMASK (0xffffffff << PDIR_ADDRBITS)
#define PFILE_ADDRBITS (32-PFILE_NODEBITS)
#define PFILE_ADDRMASK (0xffffffff << PFILE_NODEBITS)
#define PFILE_NODEMASK (0xffffffff << PFILE_ADDRBITS)

#define PDIR_BANK_SIZEOF (sizeof(struct pdir_bank) + PDIR_BANKSIZE * sizeof(struct pdir))
#define PFILE_BANK_SIZEOF (sizeof(struct pfile_bank) + PFILE_BANKSIZE * sizeof(struct pfile))

struct pdir_tuple pathman_mkdir(pathman_t * pman);
struct pfile_tuple pathman_mkfile(pathman_t * pman);
void pathman_remdir(pathman_t * pman, uint32_t index);
void pathman_remfile(pathman_t * pman, uint32_t index);

inline static
struct pstate pstate(struct tnode_tuple top)
{
  union paccess acc;
  acc.composite = top.node ? top.node->data : 0;
  struct pstate ret = {
    .top = top,
    .node = acc.node,
  };
  return ret;
}

struct pdir_tuple  pdir_tuple(struct pdir * node, uint32_t index)
{
  struct pdir_tuple ret = {
    .node = node,
    .index = index,
  };

  return ret;
}

struct pfile_tuple  pfile_tuple(struct pfile * node, uint32_t index)
{
  struct pfile_tuple ret = {
    .node = node,
    .index = index,
  };

  return ret;
}

inline static
struct plookup plookup(err_t err, struct pstate state, struct pdir * dir, struct pfile * file)
{
  struct plookup ret = {
    .err = err,
    .state = state,
    .dir = dir,
    .file = file,
  };
  return ret;
}

inline static
struct pdir_bank * pdir_bank_alloc(const mem_allocator_t * a, uint32_t addr)
{
  struct pdir_bank * bank = mem_alloc(a, PDIR_BANK_SIZEOF);

  if (bank) {
    memset(bank, 0, PDIR_BANK_SIZEOF);
    bank->used = 0;
    bank->addr = addr;
  }

  return bank;
}

inline static
struct pfile_bank * pfile_bank_alloc(const mem_allocator_t * a, uint32_t addr)
{
  struct pfile_bank * bank = mem_alloc(a, PFILE_BANK_SIZEOF);

  if (bank) {
    memset(bank, 0, PFILE_BANK_SIZEOF);
    bank->used = 0;
    bank->addr = addr;
  }

  return bank;
}

inline static
struct pdir_tuple pdir_bank_mknode(struct pdir_bank * bank)
{
  if (!bank)
    return pdir_tuple(NULL, 0);

  uint32_t next = bank->used;

  if (next < PDIR_BANKSIZE) {
    struct pdir * node = &bank->nodes[next];
    memset(node, 0, sizeof(struct pdir));
    bank->used++;
    return pdir_tuple(node, IDX2INDEX(next));
  }

  return pdir_tuple(NULL, 0);
}

inline static
struct pfile_tuple pfile_bank_mknode(struct pfile_bank * bank)
{
  if (!bank)
    return pfile_tuple(NULL, 0);

  uint32_t next = bank->used;

  if (next < PFILE_BANKSIZE) {
    struct pfile * node = &bank->nodes[next];
    memset(node, 0, sizeof(struct pfile));
    bank->used++;
    return pfile_tuple(node, IDX2INDEX(next));
  }

  return pfile_tuple(NULL, 0);
}


inline static
struct pdir_tuple pdir_get(pathman_t * pman, uint32_t index)
{
  struct pdir_bank * bank;
  struct pdir      * node;

  index = INDEX2IDX(index);

  uint32_t  addr = (index & PDIR_ADDRMASK) >> PDIR_NODEBITS;
  uint32_t  idx  = (index & PDIR_NODEMASK);

  if (addr < pman->dbanks) {
    bank = pman->dirs[addr];
    node = &bank->nodes[idx];
    if (node->isused) {
      return (struct pdir_tuple) {
        .node = node,
        .index = IDX2INDEX(index),
      };
    }
  }

  return (struct pdir_tuple){
    .node = NULL,
    .index = 0,
  };
}

inline static
struct pfile_tuple pfile_get(pathman_t * pman, uint32_t index)
{
  struct pfile_bank * bank;
  struct pfile      * node;

  index = INDEX2IDX(index);

  uint32_t  addr = (index & PFILE_ADDRMASK) >> PFILE_NODEBITS;
  uint32_t  idx  = (index & PFILE_NODEMASK);

  if (addr < pman->fbanks) {
    bank = pman->files[addr];
    node = &bank->nodes[idx];
    if (node->isused) {
      return (struct pfile_tuple) {
        .node = node,
        .index = IDX2INDEX(index),
      };
    }
  }

  return (struct pfile_tuple){
    .node = NULL,
    .index = 0,
  };
}

static inline
err_t _pathman_dir_stride_alloc(pathman_t * pman, uint16_t rest, struct pdir_tuple stride[rest])
{
  struct pdir_tuple new;

  for (uint32_t n = 0; n < rest; n++) {
    new = pathman_mkdir(pman);
    if (!new.index) {
      for (uint32_t k = 0; k < n; k++) {
        stride[k].node->isused = 1;
        pathman_remdir(pman, stride[k].index);
      }
      return ERR_MEM_USE_ALLOC;
    }
    stride[n] = new;
  }
  return 0;
}

static inline
err_t _pathman_stride_alloc(pathman_t * pman, uint16_t rest, struct pfile_tuple stride[rest])
{
  struct pfile_tuple new;

  for (uint32_t n = 0; n < rest; n++) {
    new = pathman_mkfile(pman);
    if (!new.index) {
      for (uint32_t k = 0; k < n; k++) {
        stride[k].node->isused = 1;
        pathman_remfile(pman, stride[k].index);
      }
      return ERR_MEM_USE_ALLOC;
    }
    stride[n] = new;
  }
  return 0;
}

