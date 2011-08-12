#ifndef _TRIE_H
#define _TRIE_H

#include "gc/gc.h"
#include "common/err.h"
#include "common/memory.h"

typedef struct {
  uint8_t      c;
  unsigned int iskey : 1;
  unsigned int isdata : 1;
  unsigned int isused : 1;
  unsigned int strlen : 4;
  unsigned int _reserved : 1;
  unsigned     next : 24;
  unsigned     child : 24;
  __extension__ union {
    uint8_t  str[8];
    uint64_t data;
  };
} tnode_t;

#define INDEX2IDX(__index) (__index - 1)
#define IDX2INDEX(__idx)   (__idx + 1)

typedef struct tbank tbank_t;
struct tbank {
  uint32_t  start;
  uint32_t  end;
  uint32_t  length;
  tbank_t * prev;
  tbank_t * next;
  tnode_t   nodes[];
};

struct tnode_iter {
  tbank_t  * bank;
  uint32_t   idx;
};

struct tnode_tuple {
  tnode_t  * node;
  uint32_t   index;
};

typedef struct {
  gc_global_t * g;
  tbank_t * nodes;
  tbank_t * abank;  /* allocation bank */
  uint32_t  basize; /* bank allocation size */
  uint32_t  root;   /* root key */
  uint32_t  freelist;
} trie_t;


trie_t * trie_init(gc_global_t * g, trie_t * trie, uint32_t basize);
void     trie_clear(trie_t * trie);

err_t    trie_insert(trie_t * trie, uint16_t len, const uint8_t word[len], uint64_t data, bool rep);
err_t    trie_delete(trie_t * trie, uint16_t len, const uint8_t word[len]);
err_t    trie_find(trie_t * trie, uint16_t len, const uint8_t word[len], uint64_t * data);

typedef struct {
  gc_global_t        * g;
  trie_t             * trie;
  uint16_t             len;
  uint16_t             alen;
  uint8_t            * word;
  int32_t              spos;
  uint16_t             slen;
  uint16_t             aslen;
  struct tnode_tuple * stride;
  struct tnode_iter    iter;
  struct tnode_tuple   tuple;
  unsigned             fsm; /* stop = 0, child = 1, next = 2 */
  uintptr_t            data;
} trie_iter_t;

trie_iter_t * trie_iter_init(trie_t * trie, trie_iter_t * iter);
int           trie_iter_next(trie_iter_t * iter);
void          trie_iter_clear(trie_iter_t * iter);

#endif /* _TRIE_H */