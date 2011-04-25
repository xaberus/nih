#include "trie-private.h"

#define PDIR_BANK_SIZE 32
#define PFILE_BANK_SIZE 32

#define PDIR_BANK_SIZEOF(_sz) (sizeof(struct pdir_bank) + (_sz)*sizeof(struct pdir))
#define PFILE_BANK_SIZEOF(_sz) (sizeof(struct pfile_bank) + (_sz)*sizeof(struct pfile))

struct pdir_tuple pathman_mkdir(pathman_t * pman);
struct pfile_tuple pathman_mkfile(pathman_t * pman);
void pathman_remdir(pathman_t * pman, uint32_t index);
void pathman_remfile(pathman_t * pman, uint32_t index);

inline static
struct pstate pstate(struct tnode_tuple top)
{
  struct pstate ret = {
    .top = top,
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
struct pdir_iter pdir_iter(struct pdir_bank * bank, uint32_t idx)
{
  struct pdir_iter ret = {
    .bank = bank,
    .idx = idx,
  };

  return ret;
}

inline static
struct pfile_iter pfile_iter(struct pfile_bank * bank, uint32_t idx)
{
  struct pfile_iter ret = {
    .bank = bank,
    .idx = idx,
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
struct pdir_bank * pdir_bank_alloc(uint32_t start, uint32_t end, const mem_allocator_t * a)
{
  uint32_t            size = (end - start);
  struct pdir_bank * bank;

  bank = mem_alloc(a, PDIR_BANK_SIZEOF(size));

  if (bank) {
    memset(bank, 0, PDIR_BANK_SIZEOF(size));

    bank->start = start;
    bank->end = end;
  }

  return bank;
}

inline static
struct pfile_bank * pfile_bank_alloc(uint32_t start, uint32_t end, const mem_allocator_t * a)
{
  uint32_t            size = (end - start);
  struct pfile_bank * bank;

  bank = mem_alloc(a, PFILE_BANK_SIZEOF(size));

  if (bank) {
    memset(bank, 0, PFILE_BANK_SIZEOF(size));

    bank->start = start;
    bank->end = end;
  }

  return bank;
}

inline static
struct pdir_tuple pdir_bank_mknode(struct pdir_bank * bank)
{
  if (!bank)
    return pdir_tuple(NULL, 0);

  uint32_t next = bank->start + bank->length;

  if (next < bank->end) {
    struct pdir * node = &bank->nodes[bank->length];

    memset(node, 0, sizeof(struct pdir));

    bank->length++;

    return pdir_tuple(node, IDX2INDEX(next));
  }

  return pdir_tuple(NULL, 0);
}

inline static
struct pfile_tuple pfile_bank_mknode(struct pfile_bank * bank)
{
  if (!bank)
    return pfile_tuple(NULL, 0);

  uint32_t next = bank->start + bank->length;

  if (next < bank->end) {
    struct pfile * node = &bank->nodes[bank->length];

    memset(node, 0, sizeof(struct pfile));

    bank->length++;

    return pfile_tuple(node, IDX2INDEX(next));
  }

  return pfile_tuple(NULL, 0);
}


inline static
struct pdir_bank * pdir_iter_get_bank(struct pdir_iter * iter)
{
  struct pdir_bank * bank = iter->bank;
  uint32_t            idx = iter->idx;

  /* rewind */
  while (bank && (idx < bank->start || idx >= bank->end)) {
    /* note: the direction of the list is inversed! */
    if (idx < bank->start)
      bank = bank->next;
    else if (idx >= bank->end)
      bank = bank->prev;
  }

  if (bank)
    return (iter->bank = bank);

  return NULL;
}

inline static
struct pfile_bank * pfile_iter_get_bank(struct pfile_iter * iter)
{
  struct pfile_bank * bank = iter->bank;
  uint32_t            idx = iter->idx;

  /* rewind */
  while (bank && (idx < bank->start || idx >= bank->end)) {
    /* note: the direction of the list is inversed! */
    if (idx < bank->start)
      bank = bank->next;
    else if (idx >= bank->end)
      bank = bank->prev;
  }

  if (bank)
    return (iter->bank = bank);

  return NULL;
}

inline static
struct pdir_tuple pdir_iter_get(struct pdir_iter * iter, uint32_t index)
{
  struct pdir_bank * bank;
  struct pdir      * node;

  if (!index || !iter)
    return pdir_tuple(NULL, 0);

  iter->idx = INDEX2IDX(index);

  if ((bank = pdir_iter_get_bank(iter))) {
    /* get */
    node = &bank->nodes[iter->idx - bank->start];
    if (node->isused)
      return pdir_tuple(node, index);
  }

  return pdir_tuple(NULL, 0);
}

inline static
struct pfile_tuple pfile_iter_get(struct pfile_iter * iter, uint32_t index)
{
  struct pfile_bank * bank;
  struct pfile      * node;

  if (!index || !iter)
    return pfile_tuple(NULL, 0);

  iter->idx = INDEX2IDX(index);

  if ((bank = pfile_iter_get_bank(iter))) {
    /* get */
    node = &bank->nodes[iter->idx - bank->start];
    if (node->isused)
      return pfile_tuple(node, index);
  }

  return pfile_tuple(NULL, 0);
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

