#include "trie/trie-private.h"

/* tuple */ typedef struct { err_r * err; pdtuple_t pdtuple; } e_pdtuple_t;
/* tuple */ typedef struct { err_r * err; pftuple_t pftuple; } e_pftuple_t;

/* tuple */ typedef struct { err_r * err; pdbank_t * pdbank; } e_pdbank_t;
/* tuple */ typedef struct { err_r * err; pfbank_t * pfbank; } e_pfbank_t;

#define PDIR_ADDRBITS (32-PDIR_NODEBITS)
#define PDIR_ADDRMASK (0xffffffff << PDIR_NODEBITS)
#define PDIR_NODEMASK (0xffffffff << PDIR_ADDRBITS)
#define PFILE_ADDRBITS (32-PFILE_NODEBITS)
#define PFILE_ADDRMASK (0xffffffff << PFILE_NODEBITS)
#define PFILE_NODEMASK (0xffffffff << PFILE_ADDRBITS)

#define PDIR_BANK_SIZEOF (sizeof(pdbank_t) + PDIR_BANKSIZE * sizeof(pdir_t))
#define PFILE_BANK_SIZEOF (sizeof(pfbank_t) + PFILE_BANKSIZE * sizeof(pfile_t))

e_pdtuple_t pathman_mkdir(pathman_t * pman);
e_pftuple_t pathman_mkfile(pathman_t * pman);
void pathman_remdir(pathman_t * pman, uint32_t index);
void pathman_remfile(pathman_t * pman, uint32_t index);

inline static
e_pdbank_t pdir_bank_alloc(uint32_t addr)
{
  pdbank_t * bank = malloc(PDIR_BANK_SIZEOF);
  if (!bank) {
    return (e_pdbank_t) {err_return(ERR_MEM_USE_ALLOC, "malloc() failed"), NULL};
  }

  memset(bank, 0, PDIR_BANK_SIZEOF);
  bank->used = 0;
  bank->addr = addr;

  return (e_pdbank_t) {NULL, bank};
}

inline static
e_pfbank_t pfile_bank_alloc(uint32_t addr)
{
  pfbank_t * bank = malloc(PFILE_BANK_SIZEOF);
  if (!bank) {
    return (e_pfbank_t) {err_return(ERR_MEM_USE_ALLOC, "malloc() failed"), NULL};
  }

  memset(bank, 0, PFILE_BANK_SIZEOF);
  bank->used = 0;
  bank->addr = addr;

  return (e_pfbank_t) {NULL, bank};
}

inline static
pdtuple_t pdir_bank_mknode(pdbank_t * bank)
{
  uint32_t next = bank->used;
  pdir_t * node = &bank->nodes[next];
  memset(node, 0, sizeof(pdir_t));
  bank->used++;
  return (pdtuple_t) {node, IDX2INDEX(next)};
}

inline static
pftuple_t pfile_bank_mknode(pfbank_t * bank)
{
  uint32_t next = bank->used;
  pfile_t * node = &bank->nodes[next];
  memset(node, 0, sizeof(pfile_t));
  bank->used++;
  return (pftuple_t) {node, IDX2INDEX(next)};
}


inline static
pdtuple_t pdir_get(pathman_t * pman, uint32_t index)
{
  pdbank_t * bank;
  pdir_t   * node;

  index = INDEX2IDX(index);

  uint32_t  addr = (index & PDIR_ADDRMASK) >> PDIR_NODEBITS;
  uint32_t  idx  = (index & PDIR_NODEMASK);

  if (addr < pman->dbanks) {
    bank = pman->dirs[addr];
    node = &bank->nodes[idx];
    if (node->isused) {
      return (pdtuple_t) {
        .node = node,
        .index = IDX2INDEX(index),
      };
    }
  }

  return (pdtuple_t){
    .node = NULL,
    .index = 0,
  };
}

inline static
pftuple_t pfile_get(pathman_t * pman, uint32_t index)
{
  pfbank_t * bank;
  pfile_t  * node;

  index = INDEX2IDX(index);

  uint32_t  addr = (index & PFILE_ADDRMASK) >> PFILE_NODEBITS;
  uint32_t  idx  = (index & PFILE_NODEMASK);

  if (addr < pman->fbanks) {
    bank = pman->files[addr];
    node = &bank->nodes[idx];
    if (node->isused) {
      return (pftuple_t) {
        .node = node,
        .index = IDX2INDEX(index),
      };
    }
  }

  return (pftuple_t){
    .node = NULL,
    .index = 0,
  };
}

static inline
err_r * _pathman_dir_stride_alloc(pathman_t * pman, uint16_t rest, pdtuple_t stride[rest])
{
  for (uint32_t n = 0; n < rest; n++) {
    e_pdtuple_t e = pathman_mkdir(pman);
    if (e.err) {
      for (uint32_t k = 0; k < n; k++) {
        stride[k].node->isused = 1;
        pathman_remdir(pman, stride[k].index);
      }
      return err_return(ERR_MEM_USE_ALLOC, "error while allocating stride: pathman_mkdir() failed");
    }
    stride[n] = e.pdtuple;
  }
  return NULL;
}

static inline
err_r * _pathman_file_stride_alloc(pathman_t * pman, uint16_t rest, pftuple_t stride[rest])
{
  for (uint32_t n = 0; n < rest; n++) {
    e_pftuple_t e = pathman_mkfile(pman);
    if (e.err) {
      for (uint32_t k = 0; k < n; k++) {
        stride[k].node->isused = 1;
        pathman_remfile(pman, stride[k].index);
      }
      return err_return(ERR_MEM_USE_ALLOC, "error while allocating stride: pathman_mkfile() failed");
    }
    stride[n] = e.pftuple;
  }
  return NULL;
}

