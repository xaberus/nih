#include "pathman.h"

#include <string.h>

#include "pathman-private.h"

void pathman_clear(pathman_t * pman)
{
  if (pman) {
    {
      struct pdir_bank * p = pman->dirs, * n = p ? p->next : NULL;
      for (; p; p = n, n = p ? p->next : NULL) {
        uint32_t size = p->end - p->start;
        gc_mem_free(pman->trie->g, PDIR_BANK_SIZEOF(size), p);
      }
      pman->dirs = NULL;
    }

    {
      struct pfile_bank * p = pman->files, * n = p ? p->next : NULL;
      for (; p; p = n, n = p ? p->next : NULL) {
        uint32_t size = p->end - p->start;
        gc_mem_free(pman->trie->g, PFILE_BANK_SIZEOF(size), p);
      }
      pman->files = NULL;
    }

    trie_clear(pman->trie);
  }
}

pathman_t * pathman_init(gc_global_t * g, pathman_t * pman)
{
  pman->dirs = pdir_bank_alloc(g, 0, PDIR_BANK_SIZE);
  pman->dfreelist = 0;
  pman->dabank = pman->dirs;
  pman->files = pfile_bank_alloc(g, 0, PFILE_BANK_SIZE);
  pman->ffreelist = 0;
  pman->fabank = pman->files;
  trie_init(g, pman->trie, 2048);

  {
    struct pdir_tuple root;

    root = pathman_mkdir(pman);
    if (!root.index)
      goto alloc_failed;
    root.node->isused = 1;
    struct pnode      node = {
      .isdir = 1,
      .isfile = 0,
      .mode = 0,
      .data = root.index,
    };
    union paccess     acc = {
      node,
    };
    err_t             err;

    if ((err = trie_insert(pman->trie, 1, (uint8_t *) "/", acc.composite, 0)))
      goto alloc_failed;
  }

  return pman;

alloc_failed:
  pathman_clear(pman);
  return NULL;
}

struct pdir_tuple pathman_mkdir(pathman_t * pman)
{
  struct pdir_bank * bank;

  if (pman->dfreelist) { /* reuse nodes from freelist */
    struct pdir_iter   iter = pdir_iter(pman->dirs, INDEX2IDX(pman->dfreelist));
    struct pdir_bank * bank;
    struct pdir      * node;

    if ((bank = pdir_iter_get_bank(&iter))) {
      /* get */
      node = &bank->nodes[iter.idx - bank->start];
      if (!node->isused) {
        pman->dfreelist = node->next;
        memset(node, 0, sizeof(struct pdir));
        return pdir_tuple(node, IDX2INDEX(iter.idx));
      }
    }
  } else {
    struct pdir_tuple tuple = {NULL, 0};

    tuple = pdir_bank_mknode(pman->dabank);

    if (!tuple.index && pman->dabank) {
      uint32_t start = pman->dabank->end;
      uint32_t end = start + PDIR_BANK_SIZE;

      bank = pdir_bank_alloc(pman->trie->g, start, end);
      assert(bank);

      /* link bank in */

      tuple = pdir_bank_mknode(bank);
      assert(tuple.index);

      /* link bank in */
      if (pman->dirs)
        pman->dirs->prev = bank;
      bank->next = pman->dirs;
      pman->dirs = bank;
      pman->dabank = bank;

      return tuple;
    } else
      return tuple;
  }

  return pdir_tuple(NULL, 0);
}

struct pfile_tuple pathman_mkfile(pathman_t * pman)
{
  struct pfile_bank * bank;

  if (pman->ffreelist) { /* reuse nodes from freelist */
    struct pfile_iter   iter = pfile_iter(pman->files, INDEX2IDX(pman->ffreelist));
    struct pfile_bank * bank;
    struct pfile      * node;

    if ((bank = pfile_iter_get_bank(&iter))) {
      /* get */
      node = &bank->nodes[iter.idx - bank->start];
      if (!node->isused) {
        pman->ffreelist = node->next;
        memset(node, 0, sizeof(struct pfile));
        return pfile_tuple(node, IDX2INDEX(iter.idx));
      }
    }
  } else {
    struct pfile_tuple tuple = {NULL, 0};

    tuple = pfile_bank_mknode(pman->fabank);

    if (!tuple.index && pman->dabank) {
      uint32_t start = pman->dabank->end;
      uint32_t end = start + PFILE_BANK_SIZE;

      bank = pfile_bank_alloc(pman->trie->g, start, end);
      assert(bank);

      /* link bank in */

      tuple = pfile_bank_mknode(bank);
      assert(tuple.index);

      /* link bank in */
      if (pman->files)
        pman->files->prev = bank;
      bank->next = pman->files;
      pman->files = bank;
      pman->fabank = bank;

      return tuple;
    } else
      return tuple;
  }

  return pfile_tuple(NULL, 0);
}

void pathman_remdir(pathman_t * pman, uint32_t index)
{
  struct pdir_tuple tuple;
  struct pdir_iter  iter = pdir_iter(pman->dirs, 0);

  tuple = pdir_iter_get(&iter, index);

  tuple.node->next = pman->dfreelist;
  tuple.node->isused = 0;
  pman->dfreelist = tuple.index;
}

void pathman_remfile(pathman_t * pman, uint32_t index)
{
  struct pfile_tuple tuple;
  struct pfile_iter  iter = pfile_iter(pman->files, 0);

  tuple = pfile_iter_get(&iter, index);

  tuple.node->next = pman->ffreelist;
  tuple.node->isused = 0;
  pman->ffreelist = tuple.index;
}


struct plookup pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode)
{
  size_t       len;
  const char * basename = NULL;

  if (!pman || !path)
    return plookup(ERR_IN_NULL_POINTER, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  len = strlen(path);

  if (len > UINT16_MAX)
    return plookup(ERR_IN_OVERFLOW, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  if (len == 0 || path[0] != '/')
    return plookup(ERR_IN_INVALID, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  {
    for (uint16_t k = 0; k < len; k++) {
      if (path[k] == '/') {
        if (len > 1 && k < len - 1)
          basename = path + k + 1;
      }
    }

    /*dprintf(2, "path %s, bs: %s\n", path, basename);*/

    if (!basename)
      return plookup(ERR_IN_INVALID, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

    {
      trie_t           * trie = pman->trie;
      struct tnode_tuple tuple;
      uint16_t           elen = len - (basename  - path);
      uint16_t           alen = path[len - 1] != '/' ? elen + 2 : elen + 1;
      uint8_t            word[alen];

      memcpy(word + 1, basename, elen);
      word[0] = '/';
      word[alen - 1] = '/';

      tuple = trie_find_i(trie, basename  - path, (const uint8_t *) path);
      /*dprintf(2, "<%.*s> ~ f{%p,%d} [[%.*s]]\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, alen, word);*/
      if (!tuple.index)
        return plookup(ERR_NOT_FOUND, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      struct pdir_tuple parent;
      {
        union paccess    acc; acc.composite = tuple.node->data;
        struct pnode     pnode = acc.node;

        if (!pnode.isdir)
          return plookup(ERR_CORRUPTION, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

        struct pdir_iter iter[1] = {pdir_iter(pman->dirs, 0)};
        parent = pdir_iter_get(iter, pnode.data);

      }

      uint32_t           rest;
      struct tnode_iter  iter = tnode_iter(trie->nodes, 0);

      struct trie_eppoit eppoit = {
        .err = 0,
        .parent = {NULL, 0},
        .prev = {NULL, 0},
        .act = TRIE_INSERT_FAILURE,
        .i = 0,
        .tuple = {NULL, 0},
      };

      eppoit = _trie_insert_decide(trie, tuple, &iter, alen, word, 0);
      if (eppoit.err)
        return plookup(eppoit.err, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      /*dprintf(2, "e{%d,{%p,%d},{%p,%d},%d,%d,{%p,%d}}\n",
        eppoit.err,
        eppoit.parent.node, eppoit.parent.index,
        eppoit.prev.node, eppoit.prev.index,
        eppoit.act,
        eppoit.i,
        eppoit.tuple.node, eppoit.tuple.index
      );*/

      struct pdir_tuple  dir = pathman_mkdir(pman);
      if (!dir.index)
        return plookup(ERR_MEM_ALLOC, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      dir.node->isused = 1;

      struct pnode       node = {
        .isdir = 1,
        .isfile = 0,
        .mode = mode,
        .data = dir.index,
        ._reserved = 0,
      };
      union paccess      acc; acc.node = node;

      rest = trie_calc_stride_length(&eppoit, alen);
      struct tnode_tuple stride[rest];
      if (eppoit.act) {
        if ((eppoit.err = _trie_stride_alloc(trie, rest, stride))) {
          pathman_remdir(pman, dir.index);
          return plookup(eppoit.err, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
        }
      }

      /*dprintf(2, "<%.*s> ~ f{%p,%d} e{%d} d{%p,%d} ~ %lu\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, eppoit.i,
        dir.node, dir.index, acc.composite);*/

      switch (eppoit.act) {
        case TRIE_INSERT_FAILURE:
          pathman_remdir(pman, dir.index);
          return plookup(ERR_CORRUPTION, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
        case TRIE_INSERT_PREV:
          tuple = __trie_prev_append_child_append_tail(trie, eppoit.tuple,
            eppoit.prev, eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_PARENT:
          tuple = __trie_parent_append_child_append_tail(trie, eppoit.tuple,
            eppoit.parent, eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_CHILD:
          tuple = __trie_become_child_append_tail(trie, eppoit.tuple,
            eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_ROOT:
          tuple = __trie_append_tail_to_root(trie, eppoit.tuple,
            alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SET:
          eppoit.tuple.node->isdata = 1;
          eppoit.tuple.node->data = acc.composite;
          tuple = eppoit.tuple;
          break;
        case TRIE_INSERT_SPLIT_0_SET:
          tuple = __trie_split_0_set(trie, eppoit.tuple,
            eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SPLIT_N_CHILD:
          tuple = __trie_split_n_child(trie, eppoit.tuple,
            eppoit.i, eppoit.n, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SPLIT_N_NEXT:
          tuple = __trie_split_n_next(trie, eppoit.tuple,
            eppoit.i, eppoit.n, alen, word, rest, stride, acc.composite);
          break;
      }
      if (parent.index) {
        dir.node->next = parent.node->child;
        parent.node->child = dir.index;
      }
      dir.node->state = pstate(tuple);
      return plookup(0, pstate(tuple), dir.node, NULL);
    }
  }
}

int pathman_print_ff(uint16_t len, const uint8_t word[len], uint64_t data, pathman_t * pman)
{
  union paccess acc; acc.composite = data;

  struct pnode  node = acc.node;
  int           fd = 4;

  if (node.isdir) {
    struct pdir_iter  iter[1] = {pdir_iter(pman->dirs, 0)};
    struct pdir_tuple dir = pdir_iter_get(iter, node.data);

    /*dprintf(2, "FF: dir %.*s @ %u\n", len, word, node.data);
    dprintf(2, "AF: %d\n", pman->dirs->nodes[1].isused);*/

    if (dir.index) {
      dprintf(fd, " \"dir%u\" [ shape = plaintext, label = <", dir.index);
      dprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
      dprintf(fd, "<tr>");
      dprintf(fd, "<td port=\"f0\">%d</td>", dir.index);
      if (dir.node->next)
        dprintf(fd, "<td port=\"f2\">→%u</td>", dir.node->next);
      dprintf(fd, "</tr>");
      dprintf(fd, "<tr>");

      const uint8_t * basename = NULL;
      if (len > 1) {
        for (uint16_t k = 0; k < len; k++) {
          if (word[k] == '/') {
            if (len > 1 && k < len - 1)
              basename = word + k + 1;
          }
        }
      } else {
        basename = word;
      }



      dprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%.*s</td>",
          (int) (len - (basename - word) - (len > 1 ? 1 : 0)), basename);
      if (dir.node->child)
        dprintf(fd, "<td port=\"f3\">↓%u</td>", dir.node->child);
      dprintf(fd, "</tr>");
      if (dir.node->file) {
        dprintf(fd, "<tr>");
        dprintf(fd, "<td bgcolor=\"green\" port=\"f4\">%u</td>", dir.node->file);
        dprintf(fd, "</tr>");
      }
      dprintf(fd, "</table>");
      dprintf(fd, ">]\n");

      if (dir.node->child) {
        dprintf(fd, " \"dir%u\":f3 -> \"dir%u\":f0 [color=red];\n",
            dir.index, dir.node->child);
      }
      if (dir.node->next) {
        dprintf(fd, " \"dir%u\":f2 -> \"dir%u\":f0 [color=blue, minlen=0];\n",
            dir.index, dir.node->next);
      }
      if (dir.node->file) {
        dprintf(fd, " \"dir%u\":f4 -> \"file%u\":f0 [color=green];\n",
            dir.index, dir.node->file);
      }
    }

  } else if (node.isfile) {
    struct pfile_iter  iter[1] = {pfile_iter(pman->files, 0)};
    struct pfile_tuple file = pfile_iter_get(iter, node.data);

    /*dprintf(2, "FF: file %.*s @ %u\n", len, word, node.data);
    dprintf(2, "AF: %d\n", pman->files->nodes[1].isused);*/

    if (file.index) {
      dprintf(fd, " \"file%u\" [ shape = plaintext, label = <", file.index);
      dprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
      dprintf(fd, "<tr>");
      dprintf(fd, "<td port=\"f0\">%d</td>", file.index);
      if (file.node->next)
        dprintf(fd, "<td port=\"f2\">→%u</td>", file.node->next);
      dprintf(fd, "</tr>");
      dprintf(fd, "<tr>");

      const uint8_t * basename = NULL;
      if (len > 1) {
        for (uint16_t k = 0; k < len; k++) {
          if (word[k] == '/') {
            if (len > 1 && k < len - 1)
              basename = word + k + 1;
          }
        }
      } else {
        basename = word;
      }

      dprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%.*s</td>",
          (int) (len - (basename - word)), basename);
      dprintf(fd, "</tr>");
      dprintf(fd, "</table>");
      dprintf(fd, ">]\n");

      if (file.node->next) {
        dprintf(fd, " \"file%u\":f2 -> \"file%u\":f0 [color=green, minlen=0];\n",
            file.index, file.node->next);
      }
    }
  }
  return 0;
}

void pathman_print(pathman_t * pman, int fd)
{
  dprintf(fd, "digraph pathman {\n");
  dprintf(fd, " graph [rankdir = TD]\n");
  dprintf(fd, " node [fontsize = 12, fontname = \"monospace\"]\n");
  dprintf(fd, " edge []\n");

  dprintf(fd,
      " \"pathman\" [ shape = plaintext, label = <"
      "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
      "<tr><td bgcolor=\"red\">pathman</td></tr>"
      "<tr><td port=\"f0\" bgcolor=\"gray\">%u</td></tr>"
      "</table>>]\n", pman->trie->root);
  if (pman->trie->root) {
    dprintf(fd, " \"pathman\":f0 -> \"dir%u\":f0;\n", pman->trie->root);
  }

  trie_iter_t iterstor[1], * iter;

  for (iter = trie_iter_init(pman->trie, iterstor); iter && trie_iter_next(iter);) {
    pathman_print_ff(iter->len, iter->word, iter->data, pman);
  }
  trie_iter_clear(iter);

  dprintf(fd, "}\n");
}

struct plookup pathman_add_file(pathman_t * pman, struct pdir * dir, const char * name, uint8_t mode)
{
  size_t len;

  if (!pman || !name || !dir)
    return plookup(ERR_IN_NULL_POINTER, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  len = strlen(name);

  if (len > UINT16_MAX)
    return plookup(ERR_IN_OVERFLOW, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  if (len == 0)
    return plookup(ERR_IN_INVALID, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

  {
    for (uint16_t k = 0; k < len; k++) {
      if (name[k] == '/')
        return plookup(ERR_IN_INVALID, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
    }

    // dprintf(2, "FI name %s\n", name);

    {
      trie_t           * trie = pman->trie;
      struct tnode_tuple tuple = dir->state.top;

      if (!tuple.index)
        return plookup(ERR_IN_INVALID, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      struct tnode_iter  iter = tnode_iter(trie->nodes, 0);
      struct trie_eppoit eppoit = {
        .err = 0,
        .parent = {NULL, 0},
        .prev = {NULL, 0},
        .act = TRIE_INSERT_FAILURE,
        .i = 0,
        .tuple = {NULL, 0},
      };
      uint16_t           alen = len + 1;
      uint8_t            word[alen];

      memcpy(word + 1, name, len);
      word[0] = '/';

      eppoit = _trie_insert_decide(trie, tuple, &iter, alen, word, 0);
      if (eppoit.err)
        return plookup(eppoit.err, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      /*dprintf(2, "e{%d,{%p,%d},{%p,%d},%d,%d,{%p,%d}}\n",
        eppoit.err,
        eppoit.parent.node, eppoit.parent.index,
        eppoit.prev.node, eppoit.prev.index,
        eppoit.act,
        eppoit.i,
        eppoit.tuple.node, eppoit.tuple.index
      );*/

      struct pfile_tuple file = pathman_mkfile(pman);
      if (!file.index)
        return plookup(ERR_MEM_ALLOC, pstate(tnode_tuple(NULL, 0)), NULL, NULL);

      file.node->isused = 1;

      struct pnode       node = {
        .isdir = 0,
        .isfile = 1,
        .mode = mode,
        .data = file.index,
        ._reserved = 0,
      };
      union paccess      acc; acc.node = node;
      uint32_t           rest;
      rest = trie_calc_stride_length(&eppoit, alen);
      struct tnode_tuple stride[rest];
      if (eppoit.act) {
        if ((eppoit.err = _trie_stride_alloc(trie, rest, stride))) {
          pathman_remfile(pman, file.index);
          return plookup(eppoit.err, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
        }
      }

      /*dprintf(2, "<%.*s> ~ f{%p,%d} e{%d} d{%p,%d} ~ %lu\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, eppoit.i,
        file.node, file.index, acc.composite);*/

      switch (eppoit.act) {
        case TRIE_INSERT_FAILURE:
          pathman_remfile(pman, file.index);
          return plookup(ERR_CORRUPTION, pstate(tnode_tuple(NULL, 0)), NULL, NULL);
        case TRIE_INSERT_PREV:
          tuple = __trie_prev_append_child_append_tail(trie, eppoit.tuple,
            eppoit.prev, eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_PARENT:
          tuple = __trie_parent_append_child_append_tail(trie, eppoit.tuple,
            eppoit.parent, eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_CHILD:
          tuple = __trie_become_child_append_tail(trie, eppoit.tuple,
            eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_ROOT:
          tuple = __trie_append_tail_to_root(trie, eppoit.tuple,
            alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SET:
          eppoit.tuple.node->isdata = 1;
          eppoit.tuple.node->data = acc.composite;
          break;
        case TRIE_INSERT_SPLIT_0_SET:
          tuple = __trie_split_0_set(trie, eppoit.tuple,
            eppoit.i, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SPLIT_N_CHILD:
          tuple = __trie_split_n_child(trie, eppoit.tuple,
            eppoit.i, eppoit.n, alen, word, rest, stride, acc.composite);
          break;
        case TRIE_INSERT_SPLIT_N_NEXT:
          tuple = __trie_split_n_next(trie, eppoit.tuple,
            eppoit.i, eppoit.n, alen, word, rest, stride, acc.composite);
          break;
      }
      // dprintf(2, "d{%p,%d}\n", dir->state.top.node, dir->state.top.index);
      file.node->next = dir->file;
      dir->file = file.index;
      return plookup(0, pstate(tuple), NULL, file.node);
    }
  }
}

/*struct plookup pathman_lookup(pathman_t * pman, const char * path)
{

}

}*/

#include "pathman-tests.c"
