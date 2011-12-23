#include "pathman.h"

#include <string.h>

#include "pathman-private.h"

void pathman_clear(pathman_t * pman)
{
  if (pman) {
    {
      pman->dfreelist = 0;
      for (uint32_t n = 0; n < pman->dbanks; n++) {
        mem_free(pman->trie->a, pman->dirs[n]);
      }
      mem_free(pman->trie->a, pman->dirs);
      pman->dirs = NULL;
      pman->dbanks = 0;
      pman->dabank = NULL;
    }

    {
      pman->ffreelist = 0;
      for (uint32_t n = 0; n < pman->fbanks; n++) {
        mem_free(pman->trie->a, pman->files[n]);
      }
      mem_free(pman->trie->a, pman->files);
      pman->files = NULL;
      pman->fbanks = 0;
      pman->fabank = NULL;
    }

    trie_clear(pman->trie);
  }
}

pathman_t * pathman_init(const mem_allocator_t * a, pathman_t * pman)
{
  if (!trie_init(a, pman->trie, 10))
    return NULL;

  /* pre-initialize here, clear() would fail otherwise */
  pman->dbanks = 0;
  pman->dirs = NULL;
  pman->fbanks = 0;
  pman->files = NULL;

  pman->dirs = mem_alloc(a, sizeof(struct pdir_bank *));
  if (!pman->dirs)
    goto alloc_failed;

  pman->files = mem_alloc(a, sizeof(struct pfile_bank *));
  if (!pman->files)
    goto alloc_failed;

  pman->dirs[0] = pdir_bank_alloc(a, 0);
  if (!pman->dirs[0])
    goto alloc_failed;
  pman->dbanks = 1;
  pman->dfreelist = 0;
  pman->dabank = pman->dirs[0];

  pman->files[0] = pfile_bank_alloc(a, 0);
  if (!pman->files[0])
    goto alloc_failed;
  pman->fbanks = 1;
  pman->ffreelist = 0;
  pman->fabank = pman->files[0];

  {
    struct pdir_tuple root;

    root = pathman_mkdir(pman);
    if (!root.index)
      goto alloc_failed;

    root.node->isused = 1;
    struct pnode node = {
      .isdir = 1,
      .isfile = 0,
      .mode = 0,
      .data = root.index,
    };
    union paccess acc = {
      node,
    };
    err_t err;

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
  struct pdir_bank * bank = pman->dabank;
  uint16_t  banks = pman->dbanks;

  if (pman->dfreelist) { /* reuse nodes from freelist */
    struct pdir * node;
    uint32_t index = INDEX2IDX(pman->dfreelist);
    uint32_t addr = (index & PDIR_ADDRMASK) >> PDIR_NODEBITS;
    uint32_t idx  = (index & PDIR_NODEMASK);

    if (addr < banks && idx < PDIR_BANKSIZE) {
      bank = pman->dirs[addr];
      node = &bank->nodes[idx];
      if (!node->isused) {
        pman->dfreelist = node->next;
        return (struct pdir_tuple) {
          .node = node,
          .index = IDX2INDEX(index),
        };
      }
    }
  } else {
    struct pdir_tuple tuple = pdir_bank_mknode(bank);

    if (!tuple.index && bank) {
      {
        struct pdir_bank ** tmp = mem_realloc(
          pman->trie->a,
          pman->dirs,
          sizeof(struct pdir_bank *) * banks,
          sizeof(struct pdir_bank *) * (banks + 1)
        );
        if (!tmp)
          return pdir_tuple(NULL, 0);
        pman->dirs = tmp;
      }

      bank = pdir_bank_alloc(pman->trie->a, banks);
      if (!bank)
        return pdir_tuple(NULL, 0);

      pman->dirs[banks++] = bank;
      pman->dabank = bank;
      pman->dbanks = banks;

      tuple = pdir_bank_mknode(bank);
    }
    return tuple;
  }

  return pdir_tuple(NULL, 0);
}

struct pfile_tuple pathman_mkfile(pathman_t * pman)
{
  struct pfile_bank * bank = pman->fabank;
  uint16_t  banks = pman->fbanks;

  if (pman->ffreelist) { /* reuse nodes from freelist */
    struct pfile * node;
    uint32_t index = INDEX2IDX(pman->ffreelist);
    uint32_t  addr = (index & PFILE_ADDRMASK) >> PFILE_NODEBITS;
    uint32_t  idx  = (index & PFILE_NODEMASK);

    if (addr < banks && idx < PFILE_BANKSIZE) {
      bank = pman->files[addr];
      node = &bank->nodes[idx];
      if (!node->isused) {
        pman->ffreelist = node->next;
        return (struct pfile_tuple) {
          .node = node,
          .index = IDX2INDEX(index),
        };
      }
    }
  } else {
    struct pfile_tuple tuple = pfile_bank_mknode(bank);

    if (!tuple.index && bank) {
      {
        struct pfile_bank ** tmp = mem_realloc(
          pman->trie->a,
          pman->files,
          sizeof(struct pfile_bank *) * banks,
          sizeof(struct pfile_bank *) * (banks + 1)
        );
        if (!tmp)
          return pfile_tuple(NULL, 0);
        pman->files = tmp;
      }

      bank = pfile_bank_alloc(pman->trie->a, banks);
      if (!bank)
        return pfile_tuple(NULL, 0);

      pman->files[banks++] = bank;
      pman->fabank = bank;
      pman->fbanks = banks;

      tuple = pfile_bank_mknode(bank);
    }
    return tuple;
  }

  return pfile_tuple(NULL, 0);
}

void pathman_remdir(pathman_t * pman, uint32_t index)
{
  struct pdir_tuple tuple = pdir_get(pman, index);

  tuple.node->next = pman->dfreelist;
  tuple.node->isused = 0;
  pman->dfreelist = tuple.index;
}

void pathman_remfile(pathman_t * pman, uint32_t index)
{
  struct pfile_tuple tuple = pfile_get(pman, index);

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

        parent = pdir_get(pman, pnode.data);

      }

      uint32_t           rest;

      struct trie_eppoit eppoit = {
        .err = 0,
        .parent = {NULL, 0},
        .prev = {NULL, 0},
        .act = TRIE_INSERT_FAILURE,
        .i = 0,
        .tuple = {NULL, 0},
      };

      eppoit = _trie_insert_decide(trie, tuple, alen, word, 0);
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
    struct pdir_tuple dir = pdir_get(pman, node.data);

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
    struct pfile_tuple file = pfile_get(pman, node.data);

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

      eppoit = _trie_insert_decide(trie, tuple, alen, word, 0);
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

struct plookup pathman_lookup(pathman_t * pman, const char * path)
{
  struct tnode_tuple tuple = trie_find_i(pman->trie, strlen(path), (const uint8_t *) path);
  if (tuple.index) {
    union paccess acc;
    acc.composite = tuple.node->data;

    if (acc.node.isfile) {
      return (struct plookup){
        .err = 0,
        .state = pstate(tuple),
        .dir = NULL,
        .file = pfile_get(pman, acc.node.data).node,
      };
    } else if (acc.node.isdir) {
      return (struct plookup){
        .err = 0,
        .state = pstate(tuple),
        .dir = pdir_get(pman, acc.node.data).node,
        .file = NULL,
      };
    }
  }
  return (struct plookup){ERR_NOT_FOUND, pstate(tuple), NULL, NULL};
}

#include "pathman-tests.c"
