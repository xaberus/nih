#ifndef _TRIE_H
#define _TRIE_H

#include <stdlib.h>

#include "common/err.h"

typedef struct {
  uint8_t      c;
  unsigned iskey : 1;
  unsigned isdata : 1;
  unsigned isused : 1;
  unsigned strlen : 4;
  unsigned _reserved : 1;
  unsigned next : 24;
  unsigned child : 24;
  __extension__ union {
    uint8_t  str[8];
    uint64_t data;
  };
} tnode_t;

#define INDEX2IDX(__index) ((__index) - 1)
#define IDX2INDEX(__idx)   ((__idx) + 1)

typedef struct tbank tbank_t;
struct tbank {
  uint32_t addr;
  uint32_t used;
  tnode_t  nodes[];
};

/* invariant: index == 0 implies node == NULL */
typedef struct ttuple {
  tnode_t  * node;
  uint32_t   index;
} ttuple_t;

typedef struct {
  uint16_t  addrbits;
  uint32_t  addrmask;
  uint16_t  nodebits;
  uint32_t  nodemask;

  tbank_t ** nodes;
  uint32_t  banks;
  uint32_t  banksize; /* bank allocation size */

  tbank_t * abank;  /* allocation bank */

  uint32_t  root;   /* root key */
  uint32_t  freelist;
} trie_t;

/* (1 << nodebits) is the size of a bank in the trie, thus 0 < nodebits <= 16 */
err_r    * trie_init(trie_t * trie, uint8_t nodebits);
void       trie_clear(trie_t * trie);

err_r    * trie_insert(trie_t * trie, uint16_t len, const uint8_t word[len], uint64_t data, bool rep);
err_r    * trie_delete(trie_t * trie, uint16_t len, const uint8_t word[len]);
e_uint64_t trie_find(trie_t * trie, uint16_t len, const uint8_t word[len]);

typedef struct {
  trie_t             * trie;
  uint16_t             len;
  uint16_t             alen;
  uint8_t            * word;
  int32_t              spos;
  uint16_t             slen;
  uint16_t             aslen;
  ttuple_t * stride;
  ttuple_t   tuple;
  unsigned             fsm; /* stop = 0, child = 1, next = 2 */
  uintptr_t            data;
} titer_t;

void trie_iter_init(trie_t * trie, titer_t * iter);
int  trie_iter_next(titer_t * iter);
void trie_iter_clear(titer_t * iter);

#endif /* _TRIE_H */
