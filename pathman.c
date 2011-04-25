#include "pathman.h"

#include <string.h>

#include "pathman-private.h"

#define PDIR_BANK_SIZE 32
#define PFILE_BANK_SIZE 32

#define PDIR_BANK_SIZEOF(_sz) (sizeof(struct pdir_bank) + (_sz)*sizeof(struct pdir))
#define PFILE_BANK_SIZEOF(_sz) (sizeof(struct pfile_bank) + (_sz)*sizeof(struct pfile))

pathman_t * pathman_init(pathman_t * pman, const mem_allocator_t * a)
{
  if (!pman || !a)
    return NULL;

  pman->dirs = pdir_bank_alloc(0, PDIR_BANK_SIZE, a);
  if (!pman->dirs)
    return NULL;
  memset(pman->dirs, 0, PDIR_BANK_SIZEOF(PDIR_BANK_SIZE));

  pman->files = pfile_bank_alloc(0, PFILE_BANK_SIZE, a);
  if (!pman->files) {
    mem_free(a, pman->dirs);
    return NULL;
  }
  memset(pman->files, 0, PFILE_BANK_SIZEOF(PFILE_BANK_SIZE));

  if (!trie_init(pman->trie, a)) {
    mem_free(a, pman->dirs);
    mem_free(a, pman->files);
    return NULL;
  }


  pman->dirs = NULL;
  pman->files = NULL;

  pman->a = a;

  return pman;
}

void pathman_clear(pathman_t * pman)
{
  if (pman) {
    {
      struct pdir_bank * p = pman->dirs, * n = p ? p->next : NULL;
      for (; p; p = n, n = p ? p->next : NULL) {
        mem_free(pman->a, p);
      }
      pman->dirs = NULL;
    }

    {
      struct pfile_bank * p = pman->files, * n = p ? p->next : NULL;
      for (; p; p = n, n = p ? p->next : NULL) {
        mem_free(pman->a, p);
      }
      pman->files = NULL;
    }

    trie_clear(pman->trie);
  }
}

struct pdir_tuple pathman_mkdir(pathman_t * pman)
{
  struct pdir_bank * bank;

  if (pman->dfreelist) { /* reuse nodes from freelist */
    struct pdir_iter   iter = pdir_iter(pman->dirs, INDEX2IDX(pman->dfreelist));
    struct pdir_bank * bank;
    struct pdir      * node;

    if ((bank = pdir_iter_get_bank(&iter))) {
      /* get */
      node = &bank->nodes[iter.idx - bank->start];
      if (!node->isused) {
        pman->dfreelist = node->next;
        memset(node, 0, sizeof(struct pdir));
        return pdir_tuple(node, IDX2INDEX(iter.idx));
      }
    }
  } else {
    struct pdir_tuple tuple = pdir_tuple(NULL, 0);

    tuple = pdir_bank_mknode(pman->dabank);

    if (!tuple.index && pman->dabank) {
      uint32_t start = pman->dabank->end;
      uint32_t end = start + TNODE_BANK_SIZE;

      bank = pdir_bank_alloc(start, end, pman->a);
      if (!bank)
        return tuple;

      /* link bank in */

      tuple = pdir_bank_mknode(bank);

      if (!tuple.index) {
        mem_free(pman->a, bank);
        return tuple;
      }

      /* link bank in */
      if (pman->dirs)
        pman->dirs->prev = bank;
      bank->next = pman->dirs;
      pman->dirs = bank;
      pman->dabank = bank;

      return tuple;
    } else
      return tuple;
  }

  return pdir_tuple(NULL, 0);
}

struct pfile_tuple pathman_mkfile(pathman_t * pman)
{
  struct pfile_bank * bank;

  if (pman->ffreelist) { /* reuse nodes from freelist */
    struct pfile_iter   iter = pfile_iter(pman->files, INDEX2IDX(pman->ffreelist));
    struct pfile_bank * bank;
    struct pfile      * node;

    if ((bank = pfile_iter_get_bank(&iter))) {
      /* get */
      node = &bank->nodes[iter.idx - bank->start];
      if (!node->isused) {
        pman->ffreelist = node->next;
        memset(node, 0, sizeof(struct pfile));
        return pfile_tuple(node, IDX2INDEX(iter.idx));
      }
    }
  } else {
    struct pfile_tuple tuple = pfile_tuple(NULL, 0);

    tuple = pfile_bank_mknode(pman->fabank);

    if (!tuple.index && pman->dabank) {
      uint32_t start = pman->dabank->end;
      uint32_t end = start + TNODE_BANK_SIZE;

      bank = pfile_bank_alloc(start, end, pman->a);
      if (!bank)
        return tuple;

      /* link bank in */

      tuple = pfile_bank_mknode(bank);

      if (!tuple.index) {
        mem_free(pman->a, bank);
        return tuple;
      }

      /* link bank in */
      if (pman->files)
        pman->files->prev = bank;
      bank->next = pman->files;
      pman->files = bank;
      pman->fabank = bank;

      return tuple;
    } else
      return tuple;
  }

  return pfile_tuple(NULL, 0);
}

void pathman_remdir(pathman_t * pman, uint32_t index)
{
  struct pdir_tuple tuple;
  struct pdir_iter  iter = pdir_iter(pman->dirs, 0);

  tuple = pdir_iter_get(&iter, index);

  tuple.node->next = pman->dfreelist;
  tuple.node->isused = 0;
  pman->dfreelist = tuple.index;
}

void pathman_remfile(pathman_t * pman, uint32_t index)
{
  struct pfile_tuple tuple;
  struct pfile_iter  iter = pfile_iter(pman->files, 0);

  tuple = pfile_iter_get(&iter, index);

  tuple.node->next = pman->ffreelist;
  tuple.node->isused = 0;
  pman->ffreelist = tuple.index;
}


struct plookup pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode)
{
  if (!pman || !path)
    return plookup(ERR_IN_NULL_POINTER, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  struct pdir * dir = mem_alloc(pman->a, sizeof(struct pdir));
  if (!dir)
    return plookup(ERR_MEM_ALLOC, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
  memset(dir, 0, sizeof(struct pdir));

  err_t err;

  {
    struct pnode node = {
      .isdir = 1,
      .isfile = 0,
      .mode = mode,
    };
    union paccess acc = {
      node,
    };
  }
}

struct plookup pathman_add_file(pathman_t * pman, struct pdir * dir, const char * name, uint8_t mode)
{
}

struct plookup pathman_lookup(pathman_t * pman, const char * path)
{

}

err_t pathman_dir_foreach(pathman_t * pman, struct pdir * dir, trie_forach_t f, void * ud)
{

}

