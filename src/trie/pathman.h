#ifndef _PATHMAN_H
#define _PATHMAN_H

#include "trie/trie.h"

typedef struct pnode {
  unsigned isdir : 1;
  unsigned isfile : 1;

  unsigned mode : 4;

  unsigned _reserved : 26;
  uint32_t data;
} pnode_t;

/* storage type for trie */
union paccess {
  pnode_t  node;
  uint64_t composite;
};

/* search state */
typedef struct pstate {
  ttuple_t top;
  pnode_t  node;
} pstate_t;

typedef struct pdir {
  unsigned isused : 1;
  pstate_t state;
  uint32_t next;
  uint32_t child;
  uint32_t file;
  uint32_t rid;
} pdir_t;

typedef struct pfile {
  unsigned isused : 1;
  uint32_t next;
  uint32_t rid;
} pfile_t;

#define PDIR_NODEBITS 8
#define PDIR_BANKSIZE (1 << PDIR_NODEBITS)

#define PFILE_NODEBITS 10
#define PFILE_BANKSIZE (1 << PFILE_NODEBITS)

typedef struct pdbank {
  uint32_t addr;
  uint32_t used;
  pdir_t   nodes[PDIR_BANKSIZE];
} pdbank_t;

typedef struct pfbank {
  uint32_t addr;
  uint32_t used;
  pfile_t  nodes[PFILE_BANKSIZE];
} pfbank_t;

typedef struct pdtuple {
  pdir_t * node;
  uint32_t index;
} pdtuple_t;

typedef struct pftuple {
  pfile_t * node;
  uint32_t  index;
} pftuple_t;

typedef struct pathman {
  /* downcast: allocator goes first! */
  trie_t      trie[1];
  pdbank_t ** dirs;
  uint32_t    dbanks;
  pdbank_t  * dabank;
  uint32_t    dfreelist;

  pfbank_t ** files;
  uint32_t    fbanks;
  pfbank_t  * fabank;
  uint32_t    ffreelist;
} pathman_t;

err_r * pathman_init(pathman_t * pman);
void    pathman_clear(pathman_t * pman);

/* tuple */ typedef struct { err_r * err; pdir_t * dir; } e_pdir_t;
e_pdir_t pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode);
e_pdir_t pathman_get_dir(pathman_t * pman, const char * path);

/* tuple */ typedef struct { err_r * err; pfile_t * file; } e_pfile_t;
e_pfile_t pathman_add_file(pathman_t * pman, pdir_t * dir, const char * name, uint8_t mode);
e_pfile_t pathman_get_file(pathman_t * pman, const char * path);

#endif /* _PATHMAN_H */
