#ifndef _PATHMAN_H
#define _PATHMAN_H

#include "err.h"
#include <stdint.h>

#include "memory.h"
#include "trie.h"

struct pnode {
  unsigned isdir : 1;
  unsigned isfile : 1;

  unsigned mode : 4;

  unsigned _reserved : 26;
  uint32_t data;
};

/* storage type for trie */
union paccess {
  struct pnode node;
  uint64_t     composite;
};

/* search state */
struct pstate {
  struct tnode_tuple top;
};

struct pdir {
  unsigned      isused : 1;
  size_t        count;
  size_t        size;
  struct pstate state;
  uint32_t      next;
  uint32_t      child;
  uint32_t      file;
};

struct pfile {
  unsigned isused : 1;

  size_t   offset;
  size_t   size;
  uint32_t next;
};


struct pdir_bank {
  uint32_t           start;
  uint32_t           end;
  uint32_t           length;
  struct pdir_bank * prev;
  struct pdir_bank * next;
  struct pdir        nodes[];
};

struct pdir_iter {
  struct pdir_bank * bank;
  uint32_t           idx;
};


struct pfile_bank {
  uint32_t            start;
  uint32_t            end;
  uint32_t            length;
  struct pfile_bank * prev;
  struct pfile_bank * next;
  struct pfile        nodes[];
};

struct pfile_iter {
  struct pfile_bank * bank;
  uint32_t            idx;
};

struct pdir_tuple {
  struct pdir * node;
  uint32_t      index;
};

struct pfile_tuple {
  struct pfile * node;
  uint32_t       index;
};


struct pathman {
  trie_t                  trie[1];
  struct pdir_bank      * dirs;
  struct pdir_bank      * dabank;
  uint32_t                dfreelist;
  struct pfile_bank     * files;
  struct pfile_bank     * fabank;
  uint32_t                ffreelist;
  const mem_allocator_t * a;
};

typedef struct pathman pathman_t;

pathman_t * pathman_init(pathman_t * pman, const mem_allocator_t * a);
void        pathman_clear(pathman_t * pman);

struct plookup {
  err_t          err;
  struct pstate  state;
  struct pdir  * dir; /* either dir or file set */
  struct pfile * file;
};


struct plookup pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode);
struct plookup pathman_add_file(pathman_t * pman, struct pdir * dir, const char * name, uint8_t mode);

struct plookup pathman_lookup(pathman_t * pman, const char * path);

#endif /* _PATHMAN_H */
