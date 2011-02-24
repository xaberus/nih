#ifndef _TRIE_H
#define _TRIE_H

#include "err.h"
#include <stdint.h>

enum trie_error {
  TRIE_ERROR_SUCCESS = 0,
  TRIE_ERROR_DUPLICATE,
  TRIE_ERROR_CORRUPTION,

};

struct tnode {
  uint8_t c;
  __extension__ struct {
    unsigned int iskey : 1;
    unsigned int isdata : 1;
    unsigned int _reserved : 6;
  };
  unsigned next : 24;
  unsigned child : 24;
  uint64_t data;
};

#define TNODE_BANK_SIZE 32

struct tnode_bank {
  struct tnode nodes[TNODE_BANK_SIZE];

  uint32_t id;
  uint32_t length;

  /* bitmask for bank usage */
  uint32_t usage;

  struct tnode_bank * prev;
  struct tnode_bank * next;
};

struct tnode_iter {
  struct tnode_bank * bank;
  uint32_t pos;
};

struct tnode_tuple {
  struct tnode * node;
  uint32_t index;
};

struct trie {
  struct tnode_bank * nodes;
  uint32_t size;

  /* root key */
  uint32_t root;

  uint32_t freelist;
};

typedef struct trie trie_t;

trie_t * trie_init(trie_t * trie);
void     trie_clear(trie_t * trie);
err_t    trie_insert(trie_t * trie, const uint8_t word[], uint16_t len, uint64_t data);
err_t    trie_delete(trie_t * trie, const uint8_t word[], uint16_t len);
err_t    trie_find(trie_t * trie, const uint8_t word[], uint16_t len, uint64_t * data);

#endif /* _TRIE_H */
