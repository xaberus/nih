#include "trie.h"
#include "trie-private.h"
#include "trie-tests.c"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>


#include <time.h>

trie_t * trie_init(gc_global_t * g, trie_t * trie, uint32_t basize)
{
  if (!trie)
    return NULL;

  memset(trie, 0, sizeof(trie_t));

  trie->nodes = tnode_bank_alloc(g, 0, basize);
  if (!trie->nodes)
    return NULL;

  trie->basize = basize;
  trie->abank = trie->nodes;
  trie->g = g;

  return trie;
}

void trie_clear(trie_t * trie)
{
  if (!trie)
    return;

  tnode_bank_safe_foreach(bank, trie->nodes) {
    uint32_t size = bank->end - bank->start;
    gc_mem_free(trie->g, sizeof(tbank_t) + sizeof(tnode_t) * size, bank);
  }

  memset(trie, 0, sizeof(trie_t));
}

struct tnode_tuple trie_mknode(trie_t * trie)
{
  tbank_t * bank;

  if (trie->freelist) { /* reuse nodes from freelist */
    struct tnode_iter   iter = tnode_iter(trie->nodes, INDEX2IDX(trie->freelist));
    tbank_t * bank;
    tnode_t      * node;

    if ((bank = tnode_iter_get_bank(&iter))) {
      /* get */
      node = &bank->nodes[iter.idx - bank->start];
      if (!node->isused) {
        trie->freelist = node->next;
        memset(node, 0, sizeof(tnode_t));
        return tnode_tuple(node, IDX2INDEX(iter.idx));
      }
    }
  } else {
    struct tnode_tuple tuple;

    tuple = tnode_bank_mknode(trie->abank);

    if (!tuple.index && trie->abank) {
      uint32_t start = trie->abank->end;
      uint32_t end = start + trie->basize;

      bank = tnode_bank_alloc(trie->g, start, end);
      assert(bank);

      /* link bank in */

      tuple = tnode_bank_mknode(bank);
      assert(tuple.index);

      /* link bank in */
      if (trie->nodes)
        trie->nodes->prev = bank;
      bank->next = trie->nodes;
      trie->nodes = bank;
      trie->abank = bank;

      return tuple;
    } else
      return tuple;
  }

  return tnode_tuple(NULL, 0);
}

void trie_remnode(trie_t * trie, uint32_t index)
{
  struct tnode_tuple tuple;
  struct tnode_iter  iter = tnode_iter(trie->nodes, 0);

  tuple = tnode_iter_get(&iter, index);

  tuple.node->next = trie->freelist;
  tuple.node->isused = 0;
  trie->freelist = tuple.index;
}

void trie_print_r(trie_t * trie, struct tnode_iter * iter, uint32_t index, int fd)
{
  struct tnode_tuple tuple = tnode_iter_get(iter, index);
  uint32_t           last;


  dprintf(fd, " subgraph \"cluster%u\" {\n", tuple.index);
  while (tuple.index) {
    dprintf(fd, " \"node%u\" [ shape = plaintext, label = <", tuple.index);
    dprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
    dprintf(fd, "<tr>");
    dprintf(fd, "<td colspan=\"3\" port=\"f0\">%u:%s%s:%d</td>",
        tuple.index, tuple.node->iskey ? "k" : "",
        tuple.node->isdata ? "d" : "", tuple.node->strlen);
    if (tuple.node->next)
      dprintf(fd, "<td port=\"f2\">→%u</td>", tuple.node->next);
    dprintf(fd, "</tr>");
    dprintf(fd, "<tr>");
    if (tuple.node->isdata)
      dprintf(fd, "<td bgcolor=\"black\" port=\"f4\"><font color=\"white\">↭%lu</font></td>",
          tuple.node->data);
    dprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%c</td>", tuple.node->c);
    if (!tuple.node->isdata)
      dprintf(fd, "<td port=\"f4\" bgcolor=\"gray\">%.*s</td>",
          tuple.node->strlen, tuple.node->str);
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
    tuple = tnode_iter_get(iter, tuple.node->next);
#if 1
    if (tuple.index)
      dprintf(fd, " \"node%u\":f2 -> \"node%u\" [color=blue, minlen=0];\n", last, tuple.index);
#endif
  }
  dprintf(fd, " pencolor = none;\n");
  dprintf(fd, " }\n");

#if 0
  dprintf(fd, " { rank=same ");
  tuple = tnode_iter_get(iter, index);
  while (tuple.index) {
    dprintf(fd, " \"node%u\" ", tuple.index);
    tuple = tnode_iter_get(iter, tuple.node->next);
  }
  dprintf(fd, " }\n");
#endif
}
void trie_print(trie_t * trie, int fd)
{
  struct tnode_iter iter = tnode_iter(trie->nodes, 0);

  dprintf(fd, "digraph trie {\n");
  dprintf(fd, " graph [rankdir = TD]\n");
  dprintf(fd, " node [fontsize = 12, fontname = \"monospace\"]\n");
  dprintf(fd, " edge []\n");

  dprintf(fd,
      " \"trie\" [ shape = plaintext, label = <"
      "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
      "<tr><td bgcolor=\"red\">trie</td></tr>"
      "<tr><td port=\"f0\" bgcolor=\"gray\">%u</td></tr>"
      "</table>>]\n", trie->root);
  if (trie->root) {
    trie_print_r(trie, &iter, trie->root, fd);
    dprintf(fd, " \"trie\":f0 -> \"node%u\":f0;\n", trie->root);
  }

  dprintf(fd, "}\n");
}

err_t trie_insert(trie_t * trie, uint16_t len, const uint8_t word[len], uint64_t data, bool rep)
{
  uint32_t           rest;

  struct tnode_iter  iter = tnode_iter(trie->nodes, 0);

  struct trie_eppoit eppoit = {
    .err = 0,
    .parent = {NULL, 0},
    .prev = {NULL, 0},
    /* in case we never enter the loop
     * action: become root in empty trie */
    .act = TRIE_INSERT_ROOT,
    .i = 0,
    .n = 0,
    .tuple = {NULL, 0},
  };

  if (!trie || !word)
    return ERR_IN_NULL_POINTER;
  if (!len)
    return ERR_IN_INVALID;

  if (trie->root) {
    eppoit.tuple = tnode_iter_get(&iter, trie->root);

    eppoit = _trie_insert_decide(trie, eppoit.tuple, &iter, len, word, rep);
    if (eppoit.err)
      return eppoit.err;
  }

  /* allocate space for the rest of our key
   * and roll back on failure... */

  rest = trie_calc_stride_length(&eppoit, len);;
  struct tnode_tuple stride[rest];
  if (eppoit.act) {
    if ((eppoit.err = _trie_stride_alloc(trie, rest, stride)))
      return eppoit.err;
  }

  /* _everything_ should be fine now */

  switch (eppoit.act) {
    case TRIE_INSERT_PREV:
      __trie_prev_append_child_append_tail(trie, eppoit.tuple,
        eppoit.prev, eppoit.i, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_PARENT:
      __trie_parent_append_child_append_tail(trie, eppoit.tuple,
        eppoit.parent, eppoit.i, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_CHILD:
      __trie_become_child_append_tail(trie, eppoit.tuple,
        eppoit.i, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_ROOT:
      __trie_append_tail_to_root(trie, eppoit.tuple,
        len, word, rest, stride, data);
      break;
    case TRIE_INSERT_SET:
      eppoit.tuple.node->isdata = 1;
      eppoit.tuple.node->data = data;
      break;
    case TRIE_INSERT_SPLIT_0_SET:
      __trie_split_0_set(trie, eppoit.tuple,
        eppoit.i, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_SPLIT_N_CHILD:
      __trie_split_n_child(trie, eppoit.tuple,
        eppoit.i, eppoit.n, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_SPLIT_N_NEXT:
      __trie_split_n_next(trie, eppoit.tuple,
        eppoit.i, eppoit.n, len, word, rest, stride, data);
      break;
    case TRIE_INSERT_FAILURE:
      return ERR_FAILURE;
  }

  return 0;
}

err_t trie_delete(trie_t * trie, uint16_t len, const uint8_t word[len])
{
  int                out = 0;
  int32_t            i = 0;

  if (!trie || !word)
    return ERR_IN_NULL_POINTER;
  if (!len)
    return ERR_IN_INVALID;

  uint8_t            c;
  uint8_t            nlen;
  struct tnode_tuple tuple;
  struct tnode_iter  iter = tnode_iter(trie->nodes, 0);
  struct {
    struct tnode_tuple tuple;
    struct tnode_tuple prev;
  }                  stack[len];
  uint16_t           rlen = 0;

  memset(&stack, 0, sizeof(stack[0]) * len);

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root);
    for (i = 0, c = word[i], out = 0; tuple.index && i < len; c = word[i]) {

      if (c == tuple.node->c) {
        stack[rlen++].tuple = tuple;
        i++;
        nlen = tuple.node->strlen;
        if (i == len) {
          if (tuple.node->isdata) {
            /* found key */
            if (tuple.node->child) {
              tuple.node->isdata = 0;
            } else {
              out = 1;
            }
          }
          break;
        } else if (!tuple.node->isdata && nlen > 0) {
          /* node has a string */
          uint8_t  n;
          uint32_t m;
          for (n = 0, m = i; n < nlen && m < len; n++, m++) {
            if (word[m] != tuple.node->str[n])
              break;
          }
          if (n < nlen)
            break;
          i = m;
        }
        tuple = tnode_iter_get(&iter, tuple.node->child);
      } else {
        if (tuple.node->next) {
          stack[rlen].prev = tuple;
          tuple = tnode_iter_get(&iter, tuple.node->next);
        } else {
          return ERR_NOT_FOUND;
        }
      }
    }
  }

  /* ? */
  if (out == 1) {
    uint32_t remlen = rlen;
    for (i = rlen - 1; i >= 0; i--) {
      if (i == rlen - 1) {
        if (stack[i].tuple.node->next || stack[i].prev.index) {
          remlen = rlen - i; break;
        }
      } else {
        if (stack[i].tuple.node->isdata) {
          remlen = rlen - i - 1; break;
        } else if (stack[i].tuple.node->next) {
          remlen = rlen - i; break;
        } else if (stack[i].prev.index) {
          remlen = rlen - i; break;
        }
      }
    }

    i = rlen - remlen;

    if (i > 0 && stack[i - 1].tuple.node->child == stack[i].tuple.index) {
      stack[i - 1].tuple.node->child = stack[i].tuple.node->next;
    } else if (trie->root == stack[i].tuple.index) {
      trie->root = stack[i].tuple.node->next;
    } else if (stack[i].prev.index) {
      if (stack[i].tuple.node->next) {
        stack[i].prev.node->next = stack[i].tuple.node->next;
      } else { /* last in list */
        stack[i].prev.node->next = 0;
      }
    }

    for (uint32_t j = 0; j < remlen; j++, i++) {
      trie_remnode(trie, stack[i].tuple.index);
    }
  }

  return 0;
}


struct tnode_tuple trie_find_i(trie_t * trie, uint16_t len, const uint8_t word[len])
{
  uint32_t           i = 0;
  uint8_t            c;
  uint8_t            nlen;
  struct tnode_tuple tuple;
  struct tnode_iter  iter = tnode_iter(trie->nodes, 0);

  if (trie->root) {
    tuple = tnode_iter_get(&iter, trie->root);
    for (i = 0, c = word[i]; tuple.index && i < len; c = word[i]) {
      if (c == tuple.node->c) {
        i++;
        nlen = tuple.node->strlen;
        if (i == len) {
          if (tuple.node->isdata) {
            /* found substring */
            return tuple;
          }
        } else if (!tuple.node->isdata && nlen > 0) {
          /* node has a string */
          uint8_t  n;
          uint32_t m;
          for (n = 0, m = i; n < nlen && m < len; n++, m++) {
            if (word[m] != tuple.node->str[n])
              break;
          }
          if (n < nlen)
            break;
          i = m;
        }
        tuple = tnode_iter_get(&iter, tuple.node->child);
      } else {
        if (tuple.node->next) {
          tuple = tnode_iter_get(&iter, tuple.node->next);
        } else {
          break;
        }
      }
    }
  }

  return tnode_tuple(NULL, 0);
}

err_t trie_find(trie_t * trie, uint16_t len, const uint8_t word[len], uint64_t * data)
{
  struct tnode_tuple tuple;

  if (!trie || !word)
    return ERR_IN_NULL_POINTER;
  if (!len)
    return ERR_IN_INVALID;

  tuple = trie_find_i(trie, len, word);

  if (!tuple.index)
    return ERR_NOT_FOUND;

  if (data)
    *data = tuple.node->data;

  return 0;
}

#define ALIGN16(_size) (((_size) + 15L) & ~15L)

inline static
void tuple_print(struct tnode_tuple tu)
{
  printf("node(%u) = {c = %c, {%s%s%s}, next = %u, child = %u, data = %zu}\n",
      tu.index, tu.node->c,
      tu.node->iskey ? "K" : "",
      tu.node->isdata ? "D" : "",
      tu.node->isused ? "U" : "",
      tu.node->next, tu.node->child, tu.node->data);

}

trie_iter_t * trie_iter_init(trie_t * trie, trie_iter_t * iter)
{
  if (trie && iter) {
    iter->g = trie->g;
    iter->len = iter->alen = 0;
    iter->word = NULL;
    iter->spos = 0;
    iter->slen = iter->aslen = 0;
    iter->stride = NULL;
    iter->iter = tnode_iter(trie->nodes, 0);
    iter->tuple = tnode_iter_get(&iter->iter, trie->root);
    iter->trie = trie;

    iter->fsm = 1;

    return iter;
  }
  return NULL;
}

int trie_iter_next(trie_iter_t * iter)
{
  if (iter && iter->trie) {
    while (iter->tuple.index) {
      if (iter->fsm != 0) {
        iter->slen = iter->spos + 1;

        /* allocate */
        if (iter->slen >= iter->aslen) {
          uint16_t             oslen;
          struct tnode_tuple * tmp;
          oslen = iter->aslen;
          iter->aslen = ALIGN16(iter->slen);

          tmp = gc_mem_realloc(iter->g,
              sizeof(struct tnode_tuple) * oslen,
              sizeof(struct tnode_tuple) * iter->aslen, iter->stride);
          iter->stride = tmp;
        }

        iter->stride[iter->spos] = iter->tuple;

        if (iter->tuple.node->isdata) {
          iter->len = 0;
          for (int32_t k = 0; k < iter->slen; k++) {
            if (iter->stride[k].node->isdata)
              iter->len++;
            else
              iter->len = iter->len + 1 + iter->stride[k].node->strlen;
          }
          if (iter->len > iter->alen) {
            uint16_t  olen = iter->alen;
            uint8_t * tmp;

            iter->alen = ALIGN16(iter->len);
            tmp = gc_mem_realloc(iter->g, olen, iter->alen, iter->word);
            iter->word = tmp;
          }
          for (int32_t k = 0, j = 0; k < iter->slen; k++) {
            if (iter->stride[k].node->isdata)
              iter->word[j++] = iter->stride[k].node->c;
            else {
              iter->word[j++] = iter->stride[k].node->c;
              for (uint8_t n = 0; n < iter->stride[k].node->strlen; n++) {
                iter->word[j++] = iter->stride[k].node->str[n];
              }
            }
          }

          iter->data = iter->tuple.node->data;
          iter->fsm = 0;
          return 1;
        }
      }

      if (iter->fsm == 0) {
        iter->fsm = 1;
      }

      if (iter->fsm == 1) {
        if (iter->tuple.node->child) {
          iter->tuple = tnode_iter_get(&iter->iter, iter->tuple.node->child); iter->spos++;
          continue;
        } else {
          iter->fsm = 2;
        }
      }

      if (iter->fsm == 2) {
        /* rewind the stack */
        for (; iter->spos >= 0 && !iter->stride[iter->spos].node->next; iter->spos--);
        if (iter->spos < 0)
          return 0;
        iter->tuple = tnode_iter_get(&iter->iter, iter->stride[iter->spos].node->next); iter->fsm = 1;
      }
    }
    return 0;
  }
  return -1;
}

void trie_iter_clear(trie_iter_t * iter)
{
  if (iter) {
    gc_mem_free(iter->g, iter->aslen, iter->stride);
    gc_mem_free(iter->g, iter->alen, iter->word);
    memset(iter, 0, sizeof(iter));
  }
}
