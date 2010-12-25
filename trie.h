
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
  uint8_t  c;
  __extension__ struct {
    unsigned int iskey : 1;
    unsigned int isdata : 1;
    unsigned int _reserved : 6;
  };
  uint16_t next;
  uint16_t child;
  uint16_t data;
};

#define TNODE_BANK_SIZE 35

struct tnode_bank {
  struct tnode        nodes[TNODE_BANK_SIZE];

  /* index * TNODE_BANK_SIZE + length == pos */
  uint16_t            id;
  uint16_t            length;

  struct tnode_bank * prev;
  struct tnode_bank * next;
};

struct tnode_iter {
  struct tnode_bank * bank;
  uint16_t            pos;
};

struct tnode_tuple {
  struct tnode * node;
  uint16_t       index;
};

struct trie {
  struct tnode_bank * nodes;
  uint16_t            size;

  /* root key */
  uint16_t root;
};

typedef struct trie trie_t;

trie_t  * trie_init(trie_t * trie);
void      trie_clear(trie_t * trie);
err_t     trie_insert(trie_t * trie, const uint8_t word[], uint16_t len, uint16_t data);
err_t     trie_find(trie_t * trie, const uint8_t word[], uint16_t len, uint16_t * data);

#endif /* _TRIE_H */
