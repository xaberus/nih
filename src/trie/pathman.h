#ifndef _PATHMAN_H
#define _PATHMAN_H

#include "trie/trie.h"

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
  struct pnode       node;
};

struct pdir {
  unsigned      isused : 1;
  struct pstate state;
  uint32_t      next;
  uint32_t      child;
  uint32_t      file;
  uint32_t      rid;
};

struct pfile {
  unsigned isused : 1;
  uint32_t next;
  uint32_t rid;
};

#define PDIR_NODEBITS 8
#define PDIR_BANKSIZE (1 << PDIR_NODEBITS)

#define PFILE_NODEBITS 10
#define PFILE_BANKSIZE (1 << PFILE_NODEBITS)

struct pdir_bank {
  uint32_t    addr;
  uint32_t    used;
  struct pdir nodes[PDIR_BANKSIZE];
};

struct pfile_bank {
  uint32_t     addr;
  uint32_t     used;
  struct pfile nodes[PFILE_BANKSIZE];
};

struct pdir_tuple {
  struct pdir * node;
  uint32_t      index;
};

struct pfile_tuple {
  struct pfile * node;
  uint32_t       index;
};

typedef struct pathman pathman_t;
struct pathman {
  /* downcast: allocator goes first! */
  trie_t                  trie[1];

  struct pdir_bank     ** dirs;
  uint32_t                dbanks;
  struct pdir_bank      * dabank;
  uint32_t                dfreelist;

  struct pfile_bank    ** files;
  uint32_t                fbanks;
  struct pfile_bank     * fabank;
  uint32_t                ffreelist;
};

pathman_t * pathman_init(const mem_allocator_t * a, pathman_t * pman);
void        pathman_clear(pathman_t * pman);

struct plookup {
  err_t          err;
  struct pstate  state;
  struct pdir  * dir; /* either dir xor file set */
  struct pfile * file;
};


struct plookup pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode);
struct plookup pathman_add_file(pathman_t * pman, struct pdir * dir, const char * name, uint8_t mode);

struct plookup pathman_lookup(pathman_t * pman, const char * path);

#endif /* _PATHMAN_H */
