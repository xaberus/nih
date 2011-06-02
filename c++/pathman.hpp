/*#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fts.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <assert.h>*/

#include "trie.hpp"

namespace Util {
  struct PathMan {
    struct Node {
      unsigned isdir : 1;
      unsigned isfile : 1;

      unsigned mode : 8;

      uint32_t data;
    };

    typedef Trie<Node, 1024, char> PTrie;

    struct State {
      PTrie::Tuple top;

      State();
      State(PTrie::Epo epo);
    };

    struct Dir {
      unsigned isused : 1;
      State state;
      uint32_t next;
      uint32_t child;
      uint32_t file;
    };

    struct DirTuple {
      Dir * node;
      uint32_t index;
    };

    struct DirBank {
      uint32_t start;
      uint32_t length;

      DirBank * prev;
      DirBank * next;

      Dir nodes[32];

      DirBank(uint32_t start);
      DirTuple _mknode();
    };

    struct DirIter {
      DirBank * bank;
      uint32_t  idx;

      DirIter(DirBank * bank, uint32_t idx);
      DirBank * get_bank();
      DirTuple get_node(uint32_t index);
    };

    struct File {
      unsigned isused : 1;
      uint32_t offset;
      uint32_t size;
      uint32_t next;
    };

    struct FileTuple {
      File * node;
      uint32_t index;
    };

    struct FileBank {
      uint32_t start;
      uint32_t length;

      FileBank * prev;
      FileBank * next;

      File nodes[32];

      FileBank(uint32_t start);
      FileTuple _mknode();
    };

    struct FileIter {
      FileBank * bank;
      uint32_t  idx;

      FileIter(FileBank * bank, uint32_t idx);
      FileBank * get_bank();
      FileTuple get_node(uint32_t index);
    };

    struct LookUp {
      Error err;
      State state;
      Dir * dir;
      File * file;
    };

    /*******************************/
    PTrie      trie;

    DirBank  * dirs;
    DirBank  * adbank;
    uint32_t   dfreelist;

    FileBank * files;
    FileBank * afbank;
    uint32_t   ffreelist;

    DirTuple  root;

    /*******************************/

    PathMan();
    ~PathMan();

    /*******************************/
    DirTuple _mkdir();
    void _rmdir(uint32_t index);
    FileTuple _mkfile();
    void _rmfile(uint32_t index);
    Error _check_path(const char * path, uint32_t & len);
    Error _check_name(const char * name, uint32_t & len);
    /*******************************/

    LookUp add_dir(const char * path, uint8_t mode);
    LookUp add_file(Dir * dir, const char * name, uint8_t mode);
    LookUp add_file(const char * path, uint8_t mode);
    LookUp lookup(const char * path);

    void print(int pipe);
    Error serialize(int pipe);

    static void mkword(FTSENT * node, char * word, size_t & len);

    Error read_dir(const char * path);
  };
};

