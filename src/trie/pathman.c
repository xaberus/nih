#include "pathman.h"

#include <string.h>

#include "pathman-private.h"

void pathman_clear(pathman_t * pman)
{
  if (pman) {
    {
      pman->dfreelist = 0;
      for (uint32_t n = 0; n < pman->dbanks; n++) {
        free(pman->dirs[n]);
      }
      free(pman->dirs);
      pman->dirs = NULL;
      pman->dbanks = 0;
      pman->dabank = NULL;
    }

    {
      pman->ffreelist = 0;
      for (uint32_t n = 0; n < pman->fbanks; n++) {
        free(pman->files[n]);
      }
      free(pman->files);
      pman->files = NULL;
      pman->fbanks = 0;
      pman->fabank = NULL;
    }

    trie_clear(pman->trie);
  }
}

err_r * pathman_init(pathman_t * pman)
{
  if (trie_init(pman->trie, 10)) {
    return err_return(ERR_FAILURE, "could not init tree");
  }

  /* pre-initialize here, clear() would fail otherwise */
  pman->dbanks = 0;
  pman->dirs = NULL;
  pman->fbanks = 0;
  pman->files = NULL;

  err_r * err = NULL;

  pman->dirs = malloc(sizeof(pdbank_t *));
  if (!pman->dirs) {
    err = err_return(ERR_MEM_ALLOC, "malloc() failed");
    goto alloc_failed;
  }

  pman->files = malloc(sizeof(pfbank_t *));
  if (!pman->files) {
    err = err_return(ERR_MEM_ALLOC, "malloc() failed");
    goto alloc_failed;
  }

  {
    e_pdbank_t e = pdir_bank_alloc(0);
    if (e.err) {
      err = err_return(ERR_MEM_ALLOC, "pdir_bank_alloc() failed");
      goto alloc_failed;
    }
    pman->dirs[0] = e.pdbank;
    pman->dabank = e.pdbank;
    pman->dbanks = 1;
    pman->dfreelist = 0;
  }

  {
    e_pfbank_t e = pfile_bank_alloc(0);
    if (e.err) {
      err = err_return(ERR_MEM_ALLOC, "pfile_bank_alloc() failed");
      goto alloc_failed;
    }
    pman->files[0] = e.pfbank;
    pman->fabank = e.pfbank;
    pman->fbanks = 1;
    pman->ffreelist = 0;
  }

  {
    e_pdtuple_t e = pathman_mkdir(pman);
    if (e.err) {
      err = err_return(ERR_MEM_ALLOC, "pathman_mkdir() failed");
      goto alloc_failed;
    }

    e.pdtuple.node->isused = 1;
    union paccess acc = {
      .node = {
        .isdir = 1,
        .isfile = 0,
        .mode = 0,
        .data = e.pdtuple.index,
      },
    };

    if (trie_insert(pman->trie, 1, (uint8_t *) "/", acc.composite, 0)) {
      err = err_return(ERR_MEM_ALLOC, "trie_insert() failed");
      goto alloc_failed;
    }
  }

  return NULL;

alloc_failed:
  pathman_clear(pman);
  return err;
}

e_pdtuple_t pathman_mkdir(pathman_t * pman)
{
  pdbank_t * bank = pman->dabank;
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
        return (e_pdtuple_t) {NULL, {node, IDX2INDEX(index)}};
      }
    }
  }

  if (bank->used >= PDIR_BANKSIZE) {
    pdbank_t ** tmp = realloc(pman->dirs, sizeof(struct pdir_bank *) * (banks + 1));
    if (!tmp) {
      return (e_pdtuple_t) {err_return(ERR_MEM_REALLOC, "realloc() failed"), {NULL, 0}};
    }
    pman->dirs = tmp;

    e_pdbank_t e = pdir_bank_alloc(banks);
    if (e.err) {
      return (e_pdtuple_t) {err_return(ERR_MEM_ALLOC, "pdir_bank_alloc() failed"), {NULL, 0}};
    }
    bank = e.pdbank;

    pman->dirs[banks++] = bank;
    pman->dabank = bank;
    pman->dbanks = banks;
  }

  return (e_pdtuple_t) {NULL, pdir_bank_mknode(bank)};
}

e_pftuple_t pathman_mkfile(pathman_t * pman)
{
  pfbank_t * bank = pman->fabank;
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
        return (e_pftuple_t) {NULL, {node, IDX2INDEX(index)}};
      }
    }
  }

  if (bank->used >= PFILE_BANKSIZE) {
    pfbank_t ** tmp = realloc(pman->files, sizeof(struct pfile_bank *) * (banks + 1));
    if (!tmp) {
      return (e_pftuple_t) {err_return(ERR_MEM_REALLOC, "realloc() failed"), {NULL, 0}};
    }
    pman->files = tmp;

    e_pfbank_t e = pfile_bank_alloc(banks);
    if (e.err) {
      return (e_pftuple_t) {err_return(ERR_MEM_ALLOC, "pfile_bank_alloc() failed"), {NULL, 0}};
    }
    bank = e.pfbank;

    pman->files[banks++] = bank;
    pman->fabank = bank;
    pman->fbanks = banks;
  }

  return (e_pftuple_t) {NULL, pfile_bank_mknode(bank)};
}

void pathman_remdir(pathman_t * pman, uint32_t index)
{
  pdtuple_t tuple = pdir_get(pman, index);

  tuple.node->next = pman->dfreelist;
  tuple.node->isused = 0;
  pman->dfreelist = tuple.index;
}

void pathman_remfile(pathman_t * pman, uint32_t index)
{
  pftuple_t tuple = pfile_get(pman, index);

  tuple.node->next = pman->ffreelist;
  tuple.node->isused = 0;
  pman->ffreelist = tuple.index;
}

e_pdir_t pathman_add_dir(pathman_t * pman, const char * path, uint8_t mode)
{
  size_t       len;
  const char * basename = NULL;

  if (!path) {
    return (e_pdir_t) {err_return(ERR_IN_NULL_POINTER, "path cannot be NULL"), NULL};
  }

  len = strlen(path);

  if (len == 0 || path[0] != '/' || len > UINT16_MAX) {
    return (e_pdir_t) {err_return(ERR_IN_INVALID, "path length not in range 0 < n < UINT16_MAX"), NULL};
  }

  {
    for (uint16_t k = 0; k < len; k++) {
      if (path[k] == '/') {
        if (len > 1 && k < len - 1)
          basename = path + k + 1;
      }
    }

    /*dprintf(2, "path %s, bs: %s\n", path, basename);*/

    if (!basename) {
      return (e_pdir_t) {err_return(ERR_IN_INVALID, "path has no basename"), NULL};
    }

    {
      trie_t  * trie = pman->trie;
      uint16_t  elen = len - (basename  - path);
      uint16_t  alen = path[len - 1] != '/' ? elen + 2 : elen + 1;
      uint8_t   word[alen];

      memcpy(word + 1, basename, elen);
      word[0] = '/';
      word[alen - 1] = '/';

      e_ttuple_t e = trie_find_i(trie, basename  - path, (const uint8_t *) path);
      /*dprintf(2, "<%.*s> ~ f{%p,%d} [[%.*s]]\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, alen, word);*/
      if (e.err) {
        return (e_pdir_t) {err_return(ERR_NOT_FOUND, "non-existent path"), NULL};
      }

      pdtuple_t parent;
      {
        union paccess acc; acc.composite = e.ttuple.node->data;
        if (!acc.node.isdir) {
          return (e_pdir_t) {err_return(ERR_CORRUPTION, "path is not a dir"), NULL};
        }
        parent = pdir_get(pman, acc.node.data);
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

      eppoit = _trie_insert_decide(trie, e.ttuple, alen, word, 0);
      if (eppoit.err) {
        return (e_pdir_t) {err_return(ERR_CORRUPTION, "_trie_insert_decide() failed"), NULL};
      }

      /*dprintf(2, "e{%d,{%p,%d},{%p,%d},%d,%d,{%p,%d}}\n",
        eppoit.err,
        eppoit.parent.node, eppoit.parent.index,
        eppoit.prev.node, eppoit.prev.index,
        eppoit.act,
        eppoit.i,
        eppoit.tuple.node, eppoit.tuple.index
      );*/

      e_pdtuple_t ee = pathman_mkdir(pman);
      if (ee.err) {
        return (e_pdir_t) {err_return(ERR_MEM_ALLOC, "pathman_mkdir() failed"), NULL};
      }

      ee.pdtuple.node->isused = 1;
      union paccess acc = {
        .node = {
          .isdir = 1,
          .isfile = 0,
          .mode = mode,
          .data = ee.pdtuple.index,
          ._reserved = 0,
        }
      };

      rest = trie_calc_stride_length(&eppoit, alen);
      ttuple_t stride[rest];
      err_r * eee = _trie_stride_alloc(trie, rest, stride);
      if (eee) {
        pathman_remdir(pman, ee.pdtuple.index);
        return (e_pdir_t) {err_return(ERR_MEM_ALLOC, "_trie_stride_alloc() failed"), NULL};
      }

      /*dprintf(2, "<%.*s> ~ f{%p,%d} e{%d} d{%p,%d} ~ %lu\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, eppoit.i,
        dir.node, dir.index, acc.composite);*/

      ttuple_t tuple;
      switch (eppoit.act) {
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
        case TRIE_INSERT_FAILURE:
          break;
      }
      if (parent.index) {
        ee.pdtuple.node->next = parent.node->child;
        parent.node->child = ee.pdtuple.index;
      }
      ee.pdtuple.node->state = (pstate_t) {tuple, acc.node};
      return (e_pdir_t) {NULL, ee.pdtuple.node};
    }
  }
}

int pathman_print_ff(uint16_t len, const uint8_t word[len], uint64_t data, pathman_t * pman)
{
  union paccess acc; acc.composite = data;

  struct pnode  node = acc.node;
  int           fd = 4;

  if (node.isdir) {
    pdtuple_t dir = pdir_get(pman, node.data);

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
    pftuple_t file = pfile_get(pman, node.data);

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

  titer_t iter;

  for (trie_iter_init(pman->trie, &iter); trie_iter_next(&iter);) {
    pathman_print_ff(iter.len, iter.word, iter.data, pman);
  }
  trie_iter_clear(&iter);

  dprintf(fd, "}\n");
}

e_pfile_t pathman_add_file(pathman_t * pman, struct pdir * dir, const char * name, uint8_t mode)
{
  size_t len;

  if (!name) {
    return (e_pfile_t) {err_return(ERR_IN_NULL_POINTER, "name cannot be NULL"), NULL};
  }

  len = strlen(name);

  if (len == 0 || len > UINT16_MAX) {
    return (e_pfile_t) {err_return(ERR_IN_INVALID, "name length not in range 0 < n < UINT16_MAX"), NULL};
  }

  {
    for (uint16_t k = 0; k < len; k++) {
      if (name[k] == '/') {
        return (e_pfile_t) {err_return(ERR_IN_INVALID, "name contains '/'"), NULL};
      }
    }

    // dprintf(2, "FI name %s\n", name);

    {
      trie_t * trie = pman->trie;
      ttuple_t tuple = dir->state.top;

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
      if (eppoit.err) {
        return (e_pfile_t) {err_return(ERR_CORRUPTION, "_trie_insert_decide() failed"), NULL};
      }

      /*dprintf(2, "e{%d,{%p,%d},{%p,%d},%d,%d,{%p,%d}}\n",
        eppoit.err,
        eppoit.parent.node, eppoit.parent.index,
        eppoit.prev.node, eppoit.prev.index,
        eppoit.act,
        eppoit.i,
        eppoit.tuple.node, eppoit.tuple.index
      );*/

      e_pftuple_t ee = pathman_mkfile(pman);
      if (ee.err) {
        return (e_pfile_t) {err_return(ERR_MEM_ALLOC, "pathman_mkfile() failed"), NULL};
      }

      ee.pftuple.node->isused = 1;
      union paccess      acc = {
        .node = {
          .isdir = 0,
          .isfile = 1,
          .mode = mode,
          .data = ee.pftuple.index,
          ._reserved = 0,
        }
      };
      uint32_t rest = trie_calc_stride_length(&eppoit, alen);
      ttuple_t stride[rest];
      err_r * eee = _trie_stride_alloc(trie, rest, stride);
      if (eee) {
        pathman_remfile(pman, ee.pftuple.index);
        return (e_pfile_t) {err_return(ERR_MEM_ALLOC, "_trie_stride_alloc() failed"), NULL};
      }

      /*dprintf(2, "<%.*s> ~ f{%p,%d} e{%d} d{%p,%d} ~ %lu\n",
        (int) (basename  - path), (const uint8_t *) path, tuple.node, tuple.index, eppoit.i,
        e.pftuple.node, e.pftuple.index, acc.composite);*/

      switch (eppoit.act) {
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
        case TRIE_INSERT_FAILURE:
          break;
      }
      // dprintf(2, "d{%p,%d}\n", dir->state.top.node, dir->state.top.index);
      ee.pftuple.node->next = dir->file;
      dir->file = ee.pftuple.index;
      return (e_pfile_t) {NULL, ee.pftuple.node};
    }
  }
}

e_pdir_t pathman_get_dir(pathman_t * pman, const char * path)
{
  e_ttuple_t e = trie_find_i(pman->trie, strlen(path), (const uint8_t *) path);
  if (e.err) {
    return (e_pdir_t) {err_return(ERR_NOT_FOUND, "path not found"), NULL};
  }

  union paccess acc = {
    .composite = e.ttuple.node->data,
  };
  if (!acc.node.isdir) {
    return (e_pdir_t) {err_return(ERR_NOT_FOUND, "path is not a dir"), NULL};
  }

  return (e_pdir_t) {NULL, pdir_get(pman, acc.node.data).node};
}

e_pfile_t pathman_get_file(pathman_t * pman, const char * path)
{
  e_ttuple_t e = trie_find_i(pman->trie, strlen(path), (const uint8_t *) path);
  if (e.err) {
    return (e_pfile_t) {err_return(ERR_NOT_FOUND, "path not found"), NULL};
  }

  union paccess acc = {
    .composite = e.ttuple.node->data,
  };
  if (!acc.node.isfile) {
    return (e_pfile_t) {err_return(ERR_NOT_FOUND, "path is not a file"), NULL};
  }

  return (e_pfile_t) {NULL, pfile_get(pman, acc.node.data).node};
}

#include "pathman-tests.c"
