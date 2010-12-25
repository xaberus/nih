
#include "trie.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>


#include <time.h>

inline static
struct tnode_tuple tnode_tuple(struct tnode * node, uint16_t index)
{
  struct tnode_tuple ret = {
    .node = node,
    .index = index,
  };
  return ret;
}

struct tnode_tuple tnode_iter_get(struct tnode_iter * iter, uint16_t index, int new)
{
  if (!index)
    return tnode_tuple(NULL, 0);

  index --;

  uint16_t id = index / TNODE_BANK_SIZE;
  uint16_t idx = index % TNODE_BANK_SIZE;
  struct tnode_bank * bank = iter->bank;
  struct tnode * node;

  index ++;

  /* rewind */
  while (bank && bank->id != id) {
    if (id < bank->id)
      bank = bank->next;
    else if (id > bank->id)
      bank = bank->prev;
  }

  /* get */
  if (bank) {
    node = &bank->nodes[idx];
    if (node->c || new) {
      iter->bank = bank;
      iter->pos = index;
      return tnode_tuple(node, index);
    }
  }


  return tnode_tuple(NULL, 0);
}

trie_t * trie_init(trie_t * trie)
{
  if (!trie)
    return NULL;

  memset(trie, 0, sizeof(trie_t));

  trie->nodes = malloc(sizeof(struct tnode_bank));
  if (!trie->nodes)
    return NULL;

  memset(trie->nodes, 0, sizeof(struct tnode_bank));

  return trie;
}

void trie_clear(trie_t * trie)
{
  struct tnode_bank * bank;
  struct tnode_bank * next;

  if (!trie)
    return;

  for (
    bank = trie->nodes, next = bank ? bank->next : NULL;
    bank;
    bank = next, next = bank ? bank->next : NULL
  ) {
    free(bank);
  }

  memset(trie, 0, sizeof(trie_t));
}

struct tnode_tuple trie_mknode(trie_t * trie)
{
  struct tnode_tuple tuple;
  struct tnode_iter iter = {
    .bank = trie->nodes,
    .pos = 0,
  };


  struct tnode_bank * bank;


  tuple = tnode_iter_get(&iter, trie->size + 1, 1);
  if (!tuple.node) {
    bank = malloc(sizeof(struct tnode_bank));
    if (!bank)
      return tnode_tuple(NULL, 0);
    memset(bank, 0, sizeof(struct tnode_bank));

    trie->nodes->prev = bank;
    bank->next = trie->nodes;
    bank->id = trie->nodes->id + 1;
    tuple = tnode_iter_get(&iter, trie->size + 1, 1);
    if (!tuple.node) {
      trie->nodes->prev = NULL;
      free(bank);
      return tnode_tuple(NULL, 0);
    }

    trie->nodes = bank;
    trie->size ++;
    return tnode_tuple(tuple.node, tuple.index);
  }

  trie->size ++;
  return tnode_tuple(tuple.node, tuple.index);
}

void trie_print_r(trie_t * trie, struct tnode_iter * iter, uint16_t index)
{
  struct tnode_tuple tuple = tnode_iter_get(iter, index, 0);
  uint16_t last;

  printf(" subgraph \"cluster%u\" {\n", tuple.index);
  while (tuple.node) {
    printf(" \"node%u\" [ shape = record, label = \"", tuple.index);
    printf("<f0> %u:%s%s", tuple.index, tuple.node->iskey?"k":"", tuple.node->isdata?"d":"");
    printf("| <f1> \\{ %c \\} ", tuple.node->c);
    if (tuple.node->next)
      printf("| <f2> →%u", tuple.node->next);
    if (tuple.node->child)
      printf("| <f3> ↓%u", tuple.node->child);
    if (tuple.node->isdata)
      printf("| <f4> ↭%u", tuple.node->data);
    printf("\"]\n");

    if (tuple.node->child) {
      trie_print_r(trie, iter, tuple.node->child);
      printf(" \"node%u\":f3 -> \"node%u\":f0 [color=red];\n", tuple.index, tuple.node->child);
    }

    last = tuple.index;
    tuple = tnode_iter_get(iter, tuple.node->next, 0);
    /*if (tuple.index)
      printf(" \"node%u\":f2 -> \"node%u\":f0 [color=blue];\n", last, tuple.index);*/
  }
  printf(" }\n");

  /*printf(" { rank=same ");
  tuple = tnode_iter_get(iter, index, 0);
  while (tuple.node) {
    printf(" \"node%u\" ", tuple.index);
    tuple = tnode_iter_get(iter, tuple.node->next, 0);
  }
  printf(" }\n");*/

}
void trie_print(trie_t * trie)
{
  struct tnode_iter iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  printf("digraph trie {\n");
  printf(" graph [rankdir = LR]\n");
  printf(" node [fontsize = 16]\n");
  printf(" edge []\n");

  printf(" \"trie\" [ shape = record, label = \"trie| <f0> root(%u)\"]\n", trie->root);
  if (trie->root) {
    trie_print_r(trie, &iter, trie->root);
    printf(" \"trie\":f0 -> \"node%u\":f0;\n", trie->root);
  }

  printf("}\n");
}

err_t trie_insert(trie_t * trie, const uint8_t word[], uint16_t len, uint16_t data)
{
  int out = 4;
  int i = 0, n = 0, rest;
  uint8_t c = word[i];
  struct tnode_tuple tuple;
  struct tnode_tuple new, tmp;
  struct tnode_iter iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  if (!trie || !word || !len)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_SUCCESS);

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root, 0);
    for  (i = 0, c = word[i], out = 0; tuple.node && i < len; c = word[i]) {
      if (c == tuple.node->c) {
        i ++;
        if (i == len) {
          if (tuple.node->isdata) /* duplicte */
            return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_DUPLICATE);
          else {
            tuple.node->isdata = 1;
            tuple.node->data = data;
            return err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, TRIE_ERROR_SUCCESS);
          }
        }
        tmp = tnode_iter_get(&iter, tuple.node->child, 0);
        if (tmp.node)
          tuple = tmp;
        else {
          /* if we end up here, it means, that we already have a key, 
           * which is a prefix of our new key, so we use the free child field! */
          out = 3;
          break;
        }
      } else {
        if (tuple.node->next) {
          tuple = tnode_iter_get(&iter, tuple.node->next, 0);
        } else {
          out = 1;
          break;
        }
      }
    }
  }

  if (!out)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_CORRUPTION);

  /* allocate space for the rest of our key 
   * and roll back on failure... */
  struct tnode_tuple stride[rest = len - i];

  for (n = 0; n < rest; n++) {
    new = trie_mknode(trie);
    if (!new.node) {
      trie->size -= n;
      return err_construct(ERR_MAJ_MEMORY, ERR_MIN_MEMORY_ALLOC, TRIE_ERROR_CORRUPTION);
    }
    stride[n] = new;
  }

  n = 0;

  if (out == 1) {
    /* append child, then tail */

    for (uint16_t m = i, n=0; m < len; m++) {
      new = stride[n++];
      new.node->iskey = 1;
      new.node->c = word[m];
      if (m > i)
        tuple.node->child = new.index;
      else
        tuple.node->next = new.index;

      tuple = new;
    }

    /* mark */
    tuple.node->isdata = 1;
    tuple.node->data = data;
  }

  if (out == 3) {
    /* append tail to empty root */

    for (uint16_t m = i, n=0; m < len; m++) {
      new = stride[n++];
      new.node->iskey = 1;
      new.node->c = word[m];
      tuple.node->child = new.index;

      tuple = new;
    }

    /* mark */
    tuple.node->isdata = 1;
    tuple.node->data = data;
  }


  if (out == 4) {
    /* append tail to empty root */

    for (uint16_t m = 0, n=0; m < len; m++) {
      new = stride[n++];
      new.node->iskey = 1;
      new.node->c = word[m];
      if (m > 0)
        tuple.node->child = new.index;
      else
        trie->root = new.index;

      tuple = new;
    }

    /* mark */
    tuple.node->isdata = 1;
    tuple.node->data = data;
  }

  return err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, TRIE_ERROR_SUCCESS);
}

struct tnode_tuple trie_find_i(trie_t * trie, const uint8_t word[], uint16_t len)
{
  int out = 0;
  int i = 0;
  uint8_t c = word[i];
  struct tnode_tuple tuple;
  struct tnode_iter iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root, 0);
    for  (i = 0, c = word[i], out = 0; tuple.node && i < len; c = word[i]) {
      if (c == tuple.node->c) {
        i ++;
        if (i == len) {
          if (tuple.node->isdata)
            return tuple;
          else
            return tnode_tuple(NULL, 0);
        }
        tuple = tnode_iter_get(&iter, tuple.node->child, 0);
      } else {
        if (tuple.node->next) {
          tuple = tnode_iter_get(&iter, tuple.node->next, 0);
        } else {
          out = 1;
          break;
        }
      }
    }
  }

  if (!out) {
    if (tuple.node->isdata)
      return tuple;
  }

  return tnode_tuple(NULL, 0);
}

err_t trie_find(trie_t * trie, const uint8_t word[], uint16_t len, uint16_t * data)
{
  struct tnode_tuple tuple;

  if (!trie || !word || !len)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_SUCCESS);

  tuple = trie_find_i(trie, word, len);

  if (!tuple.node)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_NOT_FOUND, TRIE_ERROR_SUCCESS);

  if (data)
    *data = tuple.node->data;

  return err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, TRIE_ERROR_SUCCESS);
}

#include "tests/trie-tests.c"
