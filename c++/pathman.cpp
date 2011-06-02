
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fts.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <assert.h>

#include "pathman.hpp"

namespace Util {

  inline static
  uint32_t index2idx(uint32_t index)
  {
    return index - 1;
  }


  inline static
  uint32_t idx2index(uint32_t idx)
  {
    return idx + 1;
  }

  PathMan::State::State()
  {
    top = PTrie::mktuple(0,0);
  }

  PathMan::State::State(PTrie::Epo epo)
  {
    top = epo.tuple;
  }


  PathMan::DirBank::DirBank(uint32_t start): start(start), length(0), prev(0), next(0)
  {
    Dir e = {0,State(),0,0,0};
    const uint32_t len = sizeof(nodes)/sizeof(nodes[0]);

    for (uint32_t k = 0; k < len; k++)
      nodes[k] = e;
  }

  PathMan::DirTuple PathMan::DirBank::_mknode()
  {
    DirTuple dt = {0,0};
    const uint32_t len = sizeof(nodes)/sizeof(nodes[0]);
    if (length < len) {
      dt.index = idx2index(start + length);
      dt.node = &nodes[length];
      length++;
    }
    return dt;
  }

  PathMan::DirIter::DirIter(DirBank * bank, uint32_t idx) : bank(bank), idx(idx)
  {
  }

  PathMan::DirBank * PathMan::DirIter::get_bank()
  {
    register DirBank * b = bank;
    const uint32_t len = sizeof(bank->nodes)/sizeof(bank->nodes[0]);
    register uint32_t  i = idx;
    register uint32_t  bs = b->start;
    register uint32_t  be = bs + len;

    /* rewind */
    while (b && (i < (bs = b->start) || i >= (be = bs + len))) {
      /* note: the direction of the list is inversed! */
      if (i < bs)
        b = b->next;
      else if (i >= be)
        b = b->prev;
    }

    if (b)
      return (bank = b);

    return 0;
  }

  PathMan::DirTuple PathMan::DirIter::get_node(uint32_t index)
  {
    Dir * dir;
    DirBank * b;
    DirTuple dt = {0,0};

    if (index) {
      idx = index2idx(index);

      if ((b = get_bank())) {
        /* get */
        dir = &b->nodes[idx - b->start];
        if (dir->isused) {
          dt.node = dir;
          dt.index = index;
          return dt;
        }
      }
    }

    return dt;

  }

  PathMan::FileBank::FileBank(uint32_t start): start(start), length(0), prev(0), next(0)
  {
    File e = {0,0,0,0};
    const uint32_t len = sizeof(nodes)/sizeof(nodes[0]);

    for (uint32_t k = 0; k < len; k++)
      nodes[k] = e;
  }

  PathMan::FileTuple PathMan::FileBank::_mknode()
  {
    FileTuple ft = {0,0};
    const uint32_t len = sizeof(nodes)/sizeof(nodes[0]);
    if (length < len) {
      ft.index = idx2index(start + length);
      ft.node = &nodes[length];
      length++;
    }
    return ft;
  }

  PathMan::FileIter::FileIter(FileBank * bank, uint32_t idx) : bank(bank), idx(idx)
  {
  }

  PathMan::FileBank * PathMan::FileIter::get_bank()
  {
    register FileBank * b = bank;
    const uint32_t len = sizeof(bank->nodes)/sizeof(bank->nodes[0]);
    register uint32_t  i = idx;
    register uint32_t  bs = b->start;
    register uint32_t  be = bs + len;

    /* rewind */
    while (b && (i < (bs = b->start) || i >= (be = bs + len))) {
      /* note: the direction of the list is inversed! */
      if (i < bs)
        b = b->next;
      else if (i >= be)
        b = b->prev;
    }

    if (b)
      return (bank = b);

    return 0;
  }

  PathMan::FileTuple PathMan::FileIter::get_node(uint32_t index)
  {
    File * file;
    FileBank * b;
    FileTuple ft = {0,0};

    if (index) {
      idx = index2idx(index);

      if ((b = get_bank())) {
        /* get */
        file = &b->nodes[idx - b->start];
        if (file->isused) {
          ft.node = file;
          ft.index = index;
          return ft;
        }
      }
    }

    return ft;

  }

  PathMan::PathMan()
  {
    dirs = new DirBank(0);
    adbank = dirs;
    dfreelist = 0;

    files = new FileBank(0);
    afbank = files;
    ffreelist = 0;

    {
      root = _mkdir();
      if (root.index) {
        Node n = {1,0,0,root.index};
        root.node->isused = 1;
        char word[] = "/";

        PTrie::Epo epo = {SUCCESS, {0,0}, {0,0}, {0,0}, PTrie::FAIL, 0, 0};
        uint32_t  rest = trie._calc_stride_length(epo, 1);
        PTrie::Tuple stride[rest];

        if (!(epo.err = trie._stride_alloc(rest, stride))) {
          epo.tuple = trie._append_tail_to_root(epo.tuple,
            1, word, rest, stride, n);

          if (epo.err == SUCCESS) {
            root.node->state = State(epo);
          } else {
            _rmdir(root.index);
            root.index = 0;
            root.node = 0;
          }
        }
      }
    }
  }

  PathMan::~PathMan()
  {
    for (DirBank * bank = dirs, * next = bank ? bank->next : 0; bank;
        bank = next, next = bank ? bank->next : 0)
      delete bank;

    for (FileBank * bank = files, * next = bank ? bank->next : 0; bank;
        bank = next, next = bank ? bank->next : 0)
      delete bank;
  }

  /*******************************/

  PathMan::DirTuple PathMan::_mkdir()
  {
    DirBank * bank;
    DirTuple dt = {0,0};
    const uint32_t len = sizeof(bank->nodes)/sizeof(bank->nodes[0]);

    if (dfreelist) { /* reuse nodes from freelist */
      DirIter iter = DirIter(dirs, index2idx(dfreelist));
      Dir   * dir;

      if ((bank = iter.get_bank())) {
        /* get */
        dir = &bank->nodes[iter.idx - bank->start];
        if (!dir->isused) {
          dfreelist = dir->next;
          dir->next = 0;
          dt.node = dir;
          dt.index = idx2index(iter.idx);
        }
      }
    } else {
      dt = adbank->_mknode();

      if (!dt.index && adbank) {
        uint32_t start = adbank->start + len;

        bank = new DirBank(start);

        dt = bank->_mknode();

        if (!dt.index) {
          delete bank;
          return dt;
        }

        /* link bank in */
        dirs->prev = bank;
        bank->next = dirs;
        dirs = bank;
        adbank = bank;
      }
    }

    return dt;
  }

  void PathMan::_rmdir(uint32_t index)
  {
    DirIter iter    = DirIter(dirs, 0);
    DirTuple tuple = iter.get_node(index);

    if (tuple.index) {
      tuple.node->next = dfreelist;

      tuple.node->isused = 0;
      tuple.node->child = 0;
      tuple.node->file = 0;

      dfreelist = tuple.index;
    }
  }

  PathMan::FileTuple PathMan::_mkfile()
  {
    FileBank * bank;
    FileTuple ft = {0,0};
    const uint32_t len = sizeof(bank->nodes)/sizeof(bank->nodes[0]);

    if (ffreelist) { /* reuse nodes from freelist */
      FileIter iter = FileIter(files, index2idx(ffreelist));
      File   * file;

      if ((bank = iter.get_bank())) {
        /* get */
        file = &bank->nodes[iter.idx - bank->start];
        if (!file->isused) {
          ffreelist = file->next;
          file->next = 0;
          ft.node = file;
          ft.index = idx2index(iter.idx);
        }
      }
    } else {
      ft = afbank->_mknode();

      if (!ft.index && afbank) {
        uint32_t start = afbank->start + len;

        bank = new FileBank(start);

        ft = bank->_mknode();

        if (ft.index) {
          /* link bank in */
          files->prev = bank;
          bank->next = files;
          files = bank;
          afbank = bank;
        } else {
          delete bank;
        }
      }
    }

    return ft;
  }

  void PathMan::_rmfile(uint32_t index)
  {
    FileIter iter    = FileIter(files, 0);
    FileTuple tuple = iter.get_node(index);

    if (tuple.index) {
      tuple.node->next = ffreelist;

      tuple.node->isused = 0;

      ffreelist = tuple.index;
    }
  }

  Error PathMan::_check_path(const char * path, uint32_t & len)
  {
    if (!path)
      return NO_PATH;

    len = strlen(path);

    if (!len)
      return EMPTY_PATH;

    if (len > 65535)
      return PATH_TOO_LONG;

    if (path[0] != '/')
      return MALFORMED_PATH;

    return SUCCESS;
  }

  Error PathMan::_check_name(const char * name, uint32_t & len)
  {
    if (!name)
      return NO_NAME;

    len = strlen(name);

    if (!len)
      return EMPTY_NAME;

    if (len > 65535)
      return NAME_TOO_LONG;

    if (strchr(name, '/'))
      return MALFOMED_NAME;

    return SUCCESS;
  }


  /*******************************/

  PathMan::LookUp PathMan::add_dir(const char * path, uint8_t mode)
  {
    LookUp lu = {SUCCESS, State(), 0, 0};
    uint32_t     len;
    const char * basename = 0;

    lu.err = CORRUPTION;

    if (trie.root) {
      lu.err = _check_path(path, len);
      if (lu.err == SUCCESS) {
        lu.err = MALFORMED_PATH;
        if (len > 1) {
          for (uint16_t k = 0; k < len; k++) {
            if (path[k] == '/') {
              if (len > 1 && k < len - 1)
              basename = path + k + 1;
            }
          }
          lu.err = SUCCESS;
        }
      }
    }

    if (lu.err == SUCCESS) {
      {
        PTrie::Tuple tuple;
        DirTuple parent;
        uint16_t elen = len - (basename - path);
        uint16_t alen = path[len - 1] != '/' ? elen + 2 : elen + 1;
        char word[alen];

        memcpy(word + 1, basename, elen);
        word[0] = '/';
        word[alen - 1] = '/';

        tuple = trie._find_i(basename - path, path);
        if (!tuple.index) {
          lu.err = PATH_NOT_FOUND;
          goto out;
        }

        if (!tuple.node->data.isdir) {
          lu.err = NOT_A_DIR;
          goto out;
        }

        parent = DirIter(dirs, 0).get_node(tuple.node->data.data);

        DirTuple dir = _mkdir();
        if (!dir.index)
          goto out;

        Node node = {1,0,0,dir.index};
        dir.node->isused = 1;

        PTrie::Epo epo = {SUCCESS, tuple, {0,0}, {0,0}, PTrie::FAIL, 0, 0};
        epo = trie._insert(epo, alen-1, word, node, false);

        if (epo.err == SUCCESS) {
          PTrie::Epo epo1 = {epo.err, epo.tuple, {0,0}, {0,0}, PTrie::FAIL, 0, 0};
          epo1 = trie._insert(epo1, 2, word+alen-2, node, false);
          if (epo1.err == SUCCESS) {
            if (parent.index) {
              dir.node->next = parent.node->child;
              parent.node->child = dir.index;
            }
            State s = State(epo1);
            dir.node->state = s;
            lu.state = s;
            lu.dir = dir.node;
          } else {
            _rmdir(dir.index);
            lu.err = epo1.err;
          }
        }
      }
    }

  out:
    return lu;
  }

  PathMan::LookUp PathMan::add_file(Dir * dir, const char * name, uint8_t mode)
  {
    LookUp lu = {SUCCESS, State(), 0, 0};
    PTrie::Tuple tuple;
    uint32_t     len;

    lu.err = CORRUPTION;

    if (trie.root) {
      lu.err = NO_DIR;
      if (dir) {
        tuple = dir->state.top;
        lu.err = CORRUPTION;
        if (tuple.index) {
          lu.err = _check_name(name, len);
        }
      }
    }

    if (lu.err == SUCCESS) {
      {
        uint16_t alen = len + 1;
        char word[alen];

        memcpy(word + 1, name, len);
        word[0] = '/';

        FileTuple file = _mkfile();
        if (!file.index) {
          lu.err = ALLOCATION_FALILURE;
          goto out;
        }

        Node node = {0,1,0,file.index};
        file.node->isused = 1;

        PTrie::Epo epo = {SUCCESS, tuple, {0,0}, {0,0}, PTrie::FAIL, 0, 0};
        epo = trie._insert(epo, alen, word, node, false);

        if (epo.err == SUCCESS) {
          State s = State(epo);
          file.node->next = dir->file;
          dir->file = file.index;
          lu.state = s;
          lu.file = file.node;
        } else {
          _rmfile(file.index);
          lu.err = epo.err;
        }
      }
    }

  out:
    return lu;
  }

  PathMan::LookUp PathMan::add_file(const char * path, uint8_t mode)
  {
    LookUp lu = {FAILURE, State(), 0, 0};
    uint32_t len;
    const char * basename;

    if (trie.root)
      lu.err = _check_path(path, len);

    basename = NULL;
    if (len > 1) {
      for (uint16_t k = 0; k < len; k++) {
        if (path[k] == '/') {
          if (len > 1 && k < len - 1) {
            basename = path + k + 1;
            lu.err = SUCCESS;
          }
        }
      }
    } else {
      lu.err = MALFORMED_PATH;
    }

    if (lu.err == SUCCESS) {
      size_t dlen = basename - path;
      char dirname[dlen + 1];
      memcpy(dirname, path, dlen); dirname[dlen] = 0;

      lu = lookup(dirname);
      if (lu.err == SUCCESS) {
        lu = add_file(lu.dir, basename, 0);
      }
    }
    return lu;
  }


  PathMan::LookUp PathMan::lookup(const char * path)
  {
    LookUp lu = {SUCCESS, State(), 0, 0};
    uint32_t     len;

    if (!path)
      lu.err = NO_PATH;

    len = strlen(path);

    if (!len)
      lu.err = EMPTY_PATH;

    if (len > 65535)
      lu.err = PATH_TOO_LONG;


    if (lu.err == SUCCESS) {
      PTrie::Tuple tuple = trie._find_i(len, path);
      if (tuple.index) {
        Node node = tuple.node->data;
        if (node.isfile) {
          FileTuple file = FileIter(files, 0).get_node(node.data);
          lu.file = file.node;
        } else if (node.isdir) {
          DirTuple dir = DirIter(dirs, 0).get_node(node.data);
          lu.dir = dir.node;
        }
      } else {
        lu.err = NOT_FOUND;
      }
    }

    return lu;
  }

  void PathMan::print(int pipe)
  {
    FILE * fd = fdopen(pipe, "w");
    if (!fd)
      return;

    fprintf(fd, "digraph pathman {\n");
    fprintf(fd, " graph [rankdir = TD]\n");
    fprintf(fd, " node [fontsize = 12, fontname = \"monospace\"]\n");
    fprintf(fd, " edge []\n");

    fprintf(fd,
        " \"pathman\" [ shape = plaintext, label = <"
        "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
        "<tr><td bgcolor=\"red\">pathman</td></tr>"
        "<tr><td port=\"f0\" bgcolor=\"gray\">%u</td></tr>"
        "</table>>]\n", trie.root);
    if (trie.root) {
      fprintf(fd, " \"pathman\":f0 -> \"dir%u\":f0;\n", trie.root);
    }

    DirIter diter = DirIter(dirs, 0);
    DirTuple dt;
    FileIter fiter = FileIter(files, 0);
    FileTuple ft;

    for (PTrie::Iter iter = PTrie::Iter(trie); iter.go();) {
      if (iter.tuple.index) {
        Node node = iter.tuple.node->data;

        if (node.isdir && iter.word[iter.len-1] == '/') {
          dt = diter.get_node(node.data);

          if (dt.index) {
            fprintf(fd, " \"dir%u\" [ shape = plaintext, label = <", dt.index);
            fprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
            fprintf(fd, "<tr>");
            fprintf(fd, "<td port=\"f0\">%d</td>", dt.index);
            if (dt.node->next)
              fprintf(fd, "<td port=\"f2\">→%u</td>", dt.node->next);
            fprintf(fd, "</tr>");
            fprintf(fd, "<tr>");

            const char * basename = NULL;
            if (iter.len > 1) {
              for (uint16_t k = 0; k < iter.len; k++) {
                if (iter.word[k] == '/') {
                  if (iter.len > 1 && k < iter.len - 1)
                    basename = iter.word + k + 1;
                }
              }
            } else {
              basename = iter.word;
            }

            fprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%.*s</td>",
                (int) (iter.len - (basename - iter.word) - (iter.len > 1 ? 1 : 0)), basename);
            if (dt.node->child)
              fprintf(fd, "<td port=\"f3\">↓%u</td>", dt.node->child);
            fprintf(fd, "</tr>");
            if (dt.node->file) {
              fprintf(fd, "<tr>");
              fprintf(fd, "<td bgcolor=\"green\" port=\"f4\">%u</td>", dt.node->file);
              fprintf(fd, "</tr>");
            }
            fprintf(fd, "</table>");
            fprintf(fd, ">]\n");

            if (dt.node->child) {
              fprintf(fd, " \"dir%u\":f3 -> \"dir%u\":f0 [color=red];\n",
                  dt.index, dt.node->child);
            }
            if (dt.node->next) {
              fprintf(fd, " \"dir%u\":f2 -> \"dir%u\":f0 [color=blue, minlen=0];\n",
                  dt.index, dt.node->next);
            }
            if (dt.node->file) {
              fprintf(fd, " \"dir%u\":f4 -> \"file%u\":f0 [color=green];\n",
                  dt.index, dt.node->file);
            }
          }

        } else if (node.isfile) {
          ft = fiter.get_node(node.data);

          if (ft.index) {
            fprintf(fd, " \"file%u\" [ shape = plaintext, label = <", ft.index);
            fprintf(fd, "<table bgcolor=\"green\" cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
            fprintf(fd, "<tr>");
            fprintf(fd, "<td port=\"f0\">%d</td>", ft.index);
            if (ft.node->next)
              fprintf(fd, "<td port=\"f2\">→%u</td>", ft.node->next);
            fprintf(fd, "</tr>");
            fprintf(fd, "<tr><td >@%u[%u]</td></tr>", ft.node->offset, ft.node->size);
            fprintf(fd, "<tr>");

            const char * basename = NULL;
            if (iter.len > 1) {
              for (uint16_t k = 0; k < iter.len; k++) {
                if (iter.word[k] == '/') {
                  if (iter.len > 1 && k < iter.len - 1)
                    basename = iter.word + k + 1;
                }
              }
            } else {
              basename = iter.word;
            }

            fprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%.*s</td>",
                (int) (iter.len - (basename - iter.word)), basename);
            fprintf(fd, "</tr>");
            fprintf(fd, "</table>");
            fprintf(fd, ">]\n");

            if (ft.node->next) {
              fprintf(fd, " \"file%u\":f2 -> \"file%u\":f0 [color=green, minlen=0];\n",
                  ft.index, ft.node->next);
            }
          }
        }
      }
      fflush(fd);
    }

    fprintf(fd, "}\n");

    fclose(fd);
  }

  Error PathMan::serialize(int pipe)
  {
    if (fcntl(pipe, F_GETFD) != -1) {
      char phdr[] = "pman 0.0\0\0\0\0\0\0\0\0";
      char pftr[] = "\0\0\0\0pman end";
      char dbhdr[] = "dbnk 0.0";
      char fbhdr[] = "fbnk 0.0";
      write(pipe, phdr, 16);
      trie.serialize(pipe);
      for (DirBank * bank = dirs, * next = bank ? bank->next : 0; bank;
         bank = next, next = bank ? bank->next : 0) {
        write(pipe, dbhdr, 8);
        write(pipe, &bank->start, sizeof(bank->start));
        write(pipe, &bank->length, sizeof(bank->length));
        for (uint32_t k = 0; k < bank->length; k++) {
          Dir & d = bank->nodes[k];
          if (d.isused) {
            struct {
              uint32_t top;
            } prt = {d.state.top.index};
            write(pipe, &prt, sizeof(prt));
            write(pipe, &d.next, sizeof(d.next));
            write(pipe, &d.child, sizeof(d.child));
            write(pipe, &d.file, sizeof(d.file));
          }
        }
      }

      for (FileBank * bank = files, * next = bank ? bank->next : 0; bank;
         bank = next, next = bank ? bank->next : 0) {
        write(pipe, fbhdr, 8);
        write(pipe, &bank->start, sizeof(bank->start));
        write(pipe, &bank->length, sizeof(bank->length));
        for (uint32_t k = 0; k < bank->length; k++) {
          File & f = bank->nodes[k];
          if (f.isused) {
            write(pipe, &f.offset, sizeof(f.offset));
            write(pipe, &f.size, sizeof(f.size));
          }
        }
      }
      write(pipe, &root.index, sizeof(root.index));
      write(pipe, pftr, 12);
    }

    return SUCCESS;
  }

  void PathMan::mkword(FTSENT * node, char * word, size_t & len)
  {
    if (node->fts_path[0] == '.' && node->fts_path[1] == '/') {
      memcpy(word, node->fts_path + 1, node->fts_pathlen - 1);
      len = node->fts_pathlen - 1;
    } else if (node->fts_path[0] == '/') {
      size_t i = node->fts_pathlen - 1, br  = i;
      short level = node->fts_level + 1;
      const char * path = node->fts_path;
      do {
        if (path[i] == '/' && i != br) {
          level--;
        }
        i--;
      } while(level > 0 && i > 0);
      len = node->fts_pathlen - i - 1;
      if (i > 0) {
        path = path + i + 1;
      } else {
        len++;
      }
      memcpy(word, path, len);
    } else {
      memcpy(word + 1, node->fts_path, node->fts_pathlen);
      len = node->fts_pathlen + 1;
      word[0]= '/';
    }
    word[len] = 0;

    // fprintf(stderr, "word: %s -> %s\n", node->fts_path, word);
  }

  Error PathMan::read_dir(const char * path) {
    uint32_t len = strlen(path);
    char word[len+1];
    memcpy(word, path, len);
    word[len ] = 0;
    char * ppath[] = {word, 0};

    LookUp lu = lookup("/"), flu;

    if (lu.err)
      return lu.err;

    FTSENT *node;
    FTS *tree = fts_open(ppath, FTS_NOCHDIR, 0);

    if (!tree)
      return FAILURE;

    {
      size_t len;
      while ((node = fts_read(tree))) {
        char word[node->fts_pathlen + 3];

        if (node->fts_info & FTS_F) {
          mkword(node, word, len);
          flu = add_file(word, 0);

          if (flu.err)
            goto fout;

          flu.file->offset = 0;
          flu.file->size = node->fts_statp->st_size;
        } else if (node->fts_info & FTS_D) {
          mkword(node, word, len);
          if (len > 1) {
            if (word[len - 1] != '/') {
              word[len++] = '/';
            }
            word[len] = 0;

            lu = add_dir(word, 0);

            if (lu.err)
              goto dout;
          }
        }
      }
      fts_close(tree);
    }

    return SUCCESS;
  fout:
    fts_close(tree);
    return flu.err;
  dout:
    fts_close(tree);
    return lu.err;
  }

};

#if 0
using namespace Util;

void wordname(char * word, size_t & len)
{
  size_t i = len - 1, br = i;
  do {
    if (word[i] == '/' && i != br) {
      i++;
      break;
    }
    i--;
  } while(i > 0);
  if (i == 0) {
    i++;
  }
  word[i] = 0;
}

int main(int argc, char * argv[])
{
  PathMan p = PathMan();

#if TEST1
  PathMan::LookUp lu = {SUCCESS, PathMan::State(), 0, 0}, flu;
  lu = p.add_dir("abc", 0); assert(lu.err == MALFORMED_PATH);
  lu = p.add_dir("/abc/a", 0); assert(lu.err == PATH_NOT_FOUND);

  lu = p.add_dir("/abc", 0); assert(lu.err == SUCCESS);

  dlu = p.add_dir(lu.dir, "qare", 0); assert(dlu.err == SUCCESS);
  flu = p.add_file(dlu.dir, "super", 0); assert(flu.err == SUCCESS); assert(flu.file); flu.file->size = 100;

  lu = p.add_dir("/abc/a", 0); assert(lu.err == SUCCESS);
  lu = p.add_dir("/abc/b", 0); assert(lu.err == SUCCESS);
  flu = p.add_file(lu.dir, "sup", 0); assert(flu.err == SUCCESS); assert(flu.file); flu.file->size = 100;
  lu = p.add_dir("/abc/b/bb", 0); assert(lu.err == SUCCESS);

  lu = p.add_dir("/abc/a/b", 0); assert(lu.err == SUCCESS);
  lu = p.add_dir("/abc/a/c", 0); assert(lu.err == SUCCESS);
  lu = p.add_dir("/abc/a/d", 0); assert(lu.err == SUCCESS);
  lu = p.add_dir("/abc/a/e", 0); assert(lu.err == SUCCESS);

  flu = p.add_file(lu.dir, "core", 0); assert(flu.err == SUCCESS); assert(flu.file); flu.file->size = 111;
  flu = p.add_file(lu.dir, "core", 0); assert(flu.err == DUPLICATE);

  assert(p.lookup("core").err == NOT_FOUND);
  assert(p.lookup("/abc/a/b/").err == SUCCESS);
  assert(p.lookup("/abc/a/e/").err == SUCCESS);
  assert(p.lookup("/abc/a/e/core").err == SUCCESS);

  for (PathMan::PTrie::Iter iter = PathMan::PTrie::Iter(p.trie); iter.go();) {
    printf("%.*s\n", iter.len, iter.word);
  }

  p.trie.print(3);
  p.print(4);
#endif

#if TEST2
  typedef Trie<uint64_t, 1024, char> TTrie;

  TTrie t = TTrie();

  t.insert(15,"/etc/init.d/arg", 0, false);
  t.insert(15,"/etc/init.d/urg", 0, false);
  t.insert(15,"/etc/vars.d/iii", 0, false);
  t.insert(15,"/etc/vars.d/cor", 0, false);

  for (TTrie::Iter iter = TTrie::Iter(t); iter.go();) {
    printf("%.*s\n", iter.len, iter.word);
  }
#endif

#if TEST3
  PathMan::LookUp lu = {SUCCESS, PathMan::State(), 0, 0}, flu;
  char a[] = ".";
  char * paths[] = {argc > 1 ? argv[1] : a, 0};

  printf("node: %zu, tnode: %zu, dir: %zu, file: %zu\n",
    sizeof(PathMan::Node), sizeof(PathMan::PTrie::TNode),
    sizeof(PathMan::Dir), sizeof(PathMan::File));

  Error err = p.read_dir(paths[0]);
  fprintf(stderr, "err: %s\n", error_string(err));
  assert(!err);

  {
    FTS *tree = fts_open(paths, FTS_NOCHDIR, 0); assert(tree);
    FTSENT *node;
    while ((node = fts_read(tree))) {
      char word[node->fts_pathlen + 3];
      size_t len;
      p.mkword(node, word, len);

      if (node->fts_info & FTS_F) {
        lu = p.lookup(word);
        if (lu.err)
          printf("L '%s' -> %s", word, error_string(lu.err));
        assert(!lu.err); assert(lu.file);
      } else if (node->fts_info & FTS_D) {
        lu = p.lookup(word); assert(!lu.err); assert(lu.dir);
      }
    }
    assert(errno==0);
    assert((fts_close(tree))==0);
  }

  printf("<<<<<<<<<<<<<<<\n");
  for (PathMan::PTrie::Iter iter = PathMan::PTrie::Iter(p.trie); iter.go();) {
    printf("%.*s\n", iter.len, iter.word); fflush(stdout);
  }
  printf(">>>>>>>>>>>>>>>\n");

  p.serialize(3);
#endif

  Error err = p.read_dir(argc > 1 ? argv[1] : ".");
  p.serialize(3);
  assert(!err);

  return 0;
}

#endif
