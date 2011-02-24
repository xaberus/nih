#include "trie.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>


#include <time.h>

inline static
struct tnode_tuple tnode_tuple(struct tnode * node, uint32_t index)
{
  struct tnode_tuple ret = {
    .node = node,
    .index = index,
  };

  return ret;
}

struct tnode_tuple tnode_iter_get(struct tnode_iter * iter, uint32_t index, int new)
{
  if (!index)
    return tnode_tuple(NULL, 0);

  index--;

  uint32_t            id = index / TNODE_BANK_SIZE;
  uint32_t            idx = index % TNODE_BANK_SIZE;
  struct tnode_bank * bank = iter->bank;
  struct tnode      * node;

  index++;

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

  for (bank = trie->nodes, next = bank ? bank->next : NULL; bank;
       bank = next, next = bank ? bank->next : NULL) {
    free(bank);
  }

  memset(trie, 0, sizeof(trie_t));
}

struct tnode_tuple trie_mknode(trie_t * trie)
{
  struct tnode_bank * bank;
  struct tnode_tuple  tuple;
  struct tnode_iter   iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  if (trie->freelist) {
    tuple = tnode_iter_get(&iter, trie->freelist, 0);
    trie->freelist = tuple.node->next;
    memset(tuple.node, 0, sizeof(struct tnode));
    return tuple;
  }

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
    trie->size++;
    return tnode_tuple(tuple.node, tuple.index);
  }

  trie->size++;
  return tnode_tuple(tuple.node, tuple.index);
}

void trie_remnode(trie_t * trie, uint32_t index)
{
  struct tnode_tuple tuple;
  struct tnode_iter  iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  tuple = tnode_iter_get(&iter, index, 0);

  tuple.node->next = trie->freelist;
  trie->freelist = tuple.index;
}

void trie_print_r(trie_t * trie, struct tnode_iter * iter, uint32_t index, int fd)
{
  struct tnode_tuple tuple = tnode_iter_get(iter, index, 0);
  uint32_t           last;


  dprintf(fd, " subgraph \"cluster%u\" {\n", tuple.index);
  while (tuple.node) {
    dprintf(fd, " \"node%u\" [ shape = plaintext, label = <", tuple.index);
    dprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
    dprintf(fd, "<tr>");
    dprintf(fd, "<td port=\"f0\">%u:%s%s</td>",
        tuple.index, tuple.node->iskey ? "k" : "", tuple.node->isdata ? "d" : "");
    if (tuple.node->next)
      dprintf(fd, "<td port=\"f2\">→%u</td>", tuple.node->next);
    dprintf(fd, "</tr>");
    dprintf(fd, "<tr>");
    if (tuple.node->isdata)
      dprintf(fd, "<td bgcolor=\"black\" port=\"f4\"><font color=\"white\">↭%lu</font></td>",
          tuple.node->data);
    dprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%c</td>", tuple.node->c);
    if (tuple.node->child)
      dprintf(fd, "<td port=\"f3\">↓%u</td>", tuple.node->child);
    dprintf(fd, "</tr>");
    dprintf(fd, "</table>");
    dprintf(fd, ">]\n");

    if (tuple.node->child) {
      trie_print_r(trie, iter, tuple.node->child, fd);
      dprintf(fd, " \"node%u\":f3 -> \"node%u\":f0 [color=red];\n", tuple.index, tuple.node->child);
    }

    last = tuple.index;
    tuple = tnode_iter_get(iter, tuple.node->next, 0);
#if 1
    if (tuple.index)
      dprintf(fd, " \"node%u\":f2 -> \"node%u\" [color=blue, minlen=0];\n", last, tuple.index);
#endif
  }
  dprintf(fd, " }\n");

#if 0
  dprintf(fd, " { rank=same ");
  tuple = tnode_iter_get(iter, index, 0);
  while (tuple.node) {
    dprintf(fd, " \"node%u\" ", tuple.index);
    tuple = tnode_iter_get(iter, tuple.node->next, 0);
  }
  dprintf(fd, " }\n");
#endif
}
void trie_print(trie_t * trie, int fd)
{
  struct tnode_iter iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  dprintf(fd, "digraph trie {\n");
  dprintf(fd, " graph [rankdir = TD]\n");
  dprintf(fd, " node [fontsize = 12, fontname = \"monospace\"]\n");
  dprintf(fd, " edge []\n");

  dprintf(fd,
      " \"trie\" [ shape = record, label = <"
      "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
      "<tr><td bgcolor=\"red\">trie</td></tr>"
      "<tr><td port=\"f0\" bgcolor=\"gray\">%u</td></tr>"
      ">]\n", trie->root);
  if (trie->root) {
    trie_print_r(trie, &iter, trie->root, fd);
    dprintf(fd, " \"trie\":f0 -> \"node%u\":f0;\n", trie->root);
  }

  dprintf(fd, "}\n");
}

err_t trie_insert(trie_t * trie, const uint8_t word[], uint16_t len, uint64_t data)
{
  int                out = 4;
  uint32_t           i = 0, n = 0, rest;
  uint8_t            c = word[i];
  struct tnode_tuple tuple;
  struct tnode_tuple new, tmp;
  struct tnode_iter  iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  if (!trie || !word || !len)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_SUCCESS);

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root, 0);
    for (i = 0, c = word[i], out = 0; tuple.node && i < len; c = word[i]) {
      if (c == tuple.node->c) {
        i++;
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

    for (uint32_t m = i, n = 0; m < len; m++) {
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
    /* become child, then append tail */

    for (uint32_t m = i, n = 0; m < len; m++) {
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

    for (uint32_t m = 0, n = 0; m < len; m++) {
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

err_t trie_delete(trie_t * trie, const uint8_t word[], uint16_t len)
{
  int                out = 0;
  int32_t            i = 0;

  if (!trie || !word || !len)
    return err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, TRIE_ERROR_SUCCESS);

  uint8_t            c = word[i];
  struct tnode_tuple tuple;
  struct tnode_iter  iter = {
    .bank = trie->nodes,
    .pos = 0,
  };
  struct {
    struct tnode_tuple tuple;
    struct tnode_tuple prev;
  }                  stack[len];

  // bt_log("delete '%.*s'\n", len, word);

  memset(&stack, 0, sizeof(stack[0]) * len);

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root, 0);
    for (i = 0, c = word[i], out = 0; tuple.node && i < len; c = word[i]) {

      // bt_log("[d] c: %c, current: %d\n", c, tuple.index);

      if (c == tuple.node->c) {

        stack[i].tuple = tuple;

        i++;
        if (i == len) {
          if (tuple.node->isdata) {
            /* found key */
            if (tuple.node->child) {

              // bt_log("[c] clear data %lu\n", tuple.node->data);

              tuple.node->isdata = 0;
            } else {
              out = 1;
            }
          }
          break;
        }
        tuple = tnode_iter_get(&iter, tuple.node->child, 0);
      } else {
        if (tuple.node->next) {
          stack[i].prev = tuple;
          tuple = tnode_iter_get(&iter, tuple.node->next, 0);
        } else {
          return err_construct(ERR_MAJ_INVALID, ERR_MIN_NOT_FOUND, TRIE_ERROR_SUCCESS);
        }
      }
    }
  }

  /* ? */
  if (out == 1) {
    uint16_t remlen = len;

    for (i = len - 1; i >= 0; i--) {
      // bt_log("[d] %d:'%c', node: %u, prev: %u \n", i, stack[i].tuple.node->c, stack[i].tuple.index, stack[i].prev.index);
      if (i == len - 1) {
        if (stack[i].tuple.node->next || stack[i].prev.index) {
          remlen = len - i; break;
        }
      } else {
        if (stack[i].tuple.node->isdata) {
          remlen = len - i - 1; break;
        } else if (stack[i].tuple.node->next) {
          remlen = len - i; break;
        } else if (stack[i].prev.index) {
          remlen = len - i; break;
        }
      }
    }

    // bt_log("[x] data %lu (%d nodes)\n", tuple.node->data, remlen);

    i = len - remlen;

    // bt_log("[a] %u '%c' \n", stack[i].tuple.index, stack[i].tuple.node->c);

    if (i > 0 && stack[i - 1].tuple.node->child == stack[i].tuple.index) {
      // bt_log("[r] %u.child = %u\n", stack[i - 1].tuple.index, stack[i].tuple.node->next);
      stack[i - 1].tuple.node->child = stack[i].tuple.node->next;
    } else if (trie->root == stack[i].tuple.index) {
      // bt_log("[r] root = %u\n", stack[i].tuple.node->next);
      trie->root = stack[i].tuple.node->next;
    } else if (stack[i].prev.index) {
      if (stack[i].tuple.node->next) {
        // bt_log("[l] %u.next = %u\n", stack[i].prev.index, stack[i].tuple.node->next);
        stack[i].prev.node->next = stack[i].tuple.node->next;
      } else { /* last in list */
        // bt_log("[n] %u.next = 0\n", stack[i].prev.index);
        stack[i].prev.node->next = 0;
      }
    }

    for (int32_t j = 0; j < remlen; j++, i++) {
      trie_remnode(trie, stack[i].tuple.index);
    }
  }

  return err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, TRIE_ERROR_SUCCESS);
}


struct tnode_tuple trie_find_i(trie_t * trie, const uint8_t word[], uint16_t len)
{
  int                out = 0;
  uint32_t           i = 0;
  uint8_t            c = word[i];
  struct tnode_tuple tuple;
  struct tnode_iter  iter = {
    .bank = trie->nodes,
    .pos = 0,
  };

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root, 0);
    for (i = 0, c = word[i], out = 1; tuple.node && i < len; c = word[i]) {
      if (c == tuple.node->c) {
        i++;
        if (i == len) {
          out = 0;
          break; /* found substring */
        }
        tuple = tnode_iter_get(&iter, tuple.node->child, 0);
      } else {
        if (tuple.node->next) {
          tuple = tnode_iter_get(&iter, tuple.node->next, 0);
        } else {
          break;
        }
      }
    }
  }

  /* is substring a key? */
  if (!out) {
    if (tuple.node->isdata)
      return tuple;
  }

  return tnode_tuple(NULL, 0);
}

err_t trie_find(trie_t * trie, const uint8_t word[], uint16_t len, uint64_t * data)
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
