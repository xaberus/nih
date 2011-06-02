#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>


#include "memory.hpp"
#include "error.hpp"

namespace Util {
  template<typename T, unsigned N = 1024, typename K = uint8_t>
  struct Trie {
    enum Action {
      FAIL = 0,
      PREV,
      PARENT,
      CHILD,
      ROOT,
      SET,
      SPLIT_0_SET,
      SPLIT_N_CHILD,
      SPLIT_N_NEXT,
    };

#define packed  __attribute__ ((packed)) 

     struct TNode {
      K c;
      __extension__ struct {
        packed unsigned int iskey : 1;
        packed unsigned int isdata : 1;
        packed unsigned int isused : 1;
        packed unsigned int strlen : 4;
        packed unsigned int _reserved : 1;
      };
      packed unsigned next : 24;
      packed unsigned child : 24;
      __extension__ union {
        K str[8];
        T data;
      };
    };

    struct Tuple {
      TNode * node;
      uint32_t index;
    };

    static Tuple mktuple(TNode * node, uint32_t index)
    {
      Tuple tuple = {node, index};

      return tuple;
    }

    struct Bank {
      uint32_t start;
      uint32_t length;

      Bank * prev;
      Bank * next;

      TNode nodes[N];

      Bank(uint32_t start) : start(start), length(0), prev(0), next(0)
      {
        TNode e = {0, {0, 0, 0, 0, 0}, 0, 0, {{0}}};

        for (uint32_t k = 0, len = N; k < len; k++)
          nodes[k] = e;
      }

      Tuple _mknode()
      {
        if (length < N) {
          uint32_t index = idx2index(start + length);
          TNode * node = &nodes[length];

          length++;
          return mktuple(node, index);
        }

        return mktuple(0, 0);
      }
    };

    struct TNodeIter {
      Bank * bank;
      uint32_t idx;
      TNodeIter(Bank * bank, uint32_t idx) : bank(bank), idx(idx)
      {
      }

      Bank * get_bank()
      {
        register Bank   * b = bank;
        register uint32_t i = idx;
        register uint32_t bs = b->start;
        register uint32_t be = bs + N;

        /* rewind */
        while (b && (i < (bs = b->start) || i >= (be = bs + N))) {
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

      Tuple  get_node(uint32_t index)
      {
        TNode * node;
        Bank  * b;

        if (index) {
          idx = index2idx(index);

          if ((b = get_bank())) {
            /* get */
            node = &b->nodes[idx - b->start];
            if (node->isused)
              return mktuple(node, index);
          }
        }

        return mktuple(0, 0);
      }
    };

    struct Epo {
      Error err;
      Tuple tuple;
      Tuple parent;
      Tuple prev;
      Action act;
      uint16_t i;
      uint8_t n;
    };

    /* ************** fields *************** */
    Bank * nodes;
    Bank * abank; /* allocation bank */
    /* root key */
    uint32_t root;
    uint32_t freelist;
    /* ************************************* */

    Trie()
    {
      nodes = new Bank(0);
      abank = nodes;
      root = 0;
      freelist = 0;
    }

    ~Trie()
    {
      for (Bank * bank = nodes, * next = bank ? bank->next : 0; bank;
         bank = next, next = bank ? bank->next : 0) {
        delete bank;
      }
    }

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

    Tuple  _mknode()
    {
      Bank * bank;

      if (freelist) { /* reuse nodes from freelist */
        TNodeIter iter = TNodeIter(nodes, index2idx(freelist));
        TNode   * node;

        if ((bank = iter.get_bank())) {
          /* get */
          node = &bank->nodes[iter.idx - bank->start];
          if (!node->isused) {
            freelist = node->next;
            node->next = 0;
            return mktuple(node, idx2index(iter.idx));
          }
        }
      } else {
        Tuple tuple = abank->_mknode();

        if (!tuple.index && abank) {
          uint32_t start = abank->start + N;

          bank = new Bank(start);

          tuple = bank->_mknode();

          if (!tuple.index) {
            delete bank;
            return mktuple(0, 0);
          }

          /* link bank in */
          if (nodes)
            nodes->prev = bank;
          bank->next = nodes;
          nodes = bank;
          abank = bank;

          return tuple;
        }

        return tuple;
      }

      return mktuple(0, 0);
    }

    void   rmnode(uint32_t index)
    {
      TNodeIter iter = TNodeIter(nodes, 0);
      Tuple     tuple = iter.get_node(index);

      if (tuple.index) {
        tuple.node->next = freelist;

        tuple.node->iskey = 0;
        tuple.node->isdata = 0;
        tuple.node->isused = 0;
        tuple.node->strlen = 0;
        tuple.node->next = 0;
        tuple.node->child = 0;

        freelist = tuple.index;
      }
    }

    inline
    Epo _insert_decide(Tuple tuple, struct TNodeIter & iter,
      uint16_t len, const K word[/*len*/], bool rep)
    {
      Tuple    prev = {0, 0};
      Tuple    parent = {0, 0};
      Tuple    tmp = {0, 0};
      Error    err = SUCCESS;
      Action   act = FAIL;
      uint32_t i = 0;
      uint8_t  n = 0;
      uint8_t  nlen;
      K  c;

      for (i = 0, c = word[i]; tuple.index && i < len; c = word[i]) {
        if (c == tuple.node->c) {
          i++;
          nlen = tuple.node->strlen;
          if (i == len) {
            if (!rep && tuple.node->isdata) { /* duplicate */
              err = DUPLICATE;
              break;
            } else {
              if (nlen > 0) {
                /* action: detach string, set data */
                act = SPLIT_0_SET;
              } else {
                /* action: set data */
                act = SET;
              }
              break;
            }
          } else if (!tuple.node->isdata && nlen > 0) {
            /* node has a string */
            for (n = 0; n < nlen && i < len; n++, i++) {
              if (word[i] != tuple.node->str[n]) {
                act = SPLIT_N_NEXT;
                goto break_main;
              } else if (i + 1 == len) {
                act = SPLIT_N_CHILD;
                goto break_main;
              }
            }
          }
          tmp = iter.get_node(tuple.node->child);
          if (tmp.index) {
            prev = mktuple(0, 0);
            parent = tuple;
            tuple = tmp;
            continue;
          } else {
            /* if we end up here, it means, that we already have a key,
             * which is a prefix of our new key, so we use the free child field!
             * action: become child */
            act = CHILD;
            break;
          }
        } else if (c < tuple.node->c) {
          if (prev.index) {
            /* action: append child */
            act = PREV;
            break;
          } else {
            /* action: append child to parent */
            act = PARENT;
            break;
          }
        } else {
          if (tuple.node->next) {
            /* advance one child */
            prev = tuple;
            tuple = iter.get_node(tuple.node->next);
          } else {
            /* we reached the last child, action: append child */
            prev = tuple;
            act = PREV;
            break;
          }
        }
      }
  break_main:

      if (!act && err == SUCCESS)
        err = CORRUPTION;

      Epo ret = {err, tuple, parent, prev, act, (uint16_t) i, n};
      return ret;
    }

    inline
    uint16_t _calc_stride_length(Epo & epo, uint16_t len)
    {
      uint32_t rest = len - epo.i;

      uint32_t m;

      for (m = epo.i, rest = 0; m < len;) {
        /*dprintf(2, "T m:%u, rest: %u\rest", m, rest);*/
        rest++;
        m++;
        for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
          m++;
        }
      }

      if (epo.act == SPLIT_N_CHILD) {
        if (epo.n + 1 < epo.tuple.node->strlen)
          rest = 2;
        else
          rest = 1;
      } else if (epo.act == SPLIT_0_SET) {
        rest = 1;
      } else if (epo.act == SPLIT_N_NEXT) {
        rest++;
      }

      /*dprintf(2, "CLEN: act: %d, len: %u, i: %u, delta: %u => %d\n",
        epo->act, len, epo->i, len - epo->i, rest);*/

      return rest;
    }

    inline
    Error _stride_alloc(uint16_t rest, Tuple stride[/*rest*/])
    {
      Tuple nw;

      for (uint32_t n = 0; n < rest; n++) {
        nw = _mknode();
        if (!nw.index) {
          for (uint32_t k = 0; k < n; k++) {
            stride[k].node->isused = 1;
            rmnode(stride[k].index);
          }
          return ALLOCATION_FALILURE;
        }
        stride[n] = nw;
      }
      return SUCCESS;
    }

    inline
    Tuple _prev_child_append_tail(Tuple tuple, Tuple prev, uint16_t i,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple    nw;
      uint32_t m, n;

      /*dprintf(2, "IDOR T len:%u\n", len);*/

      /* append child, then tail */

      for (m = i, n = 0; m < len;) {
        /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
        assert(n < rest);
        nw = stride[n++];
        nw.node->iskey = 1;
        nw.node->isused = 1;
        if (m > i)
          tuple.node->child = nw.index;
        else {
          nw.node->next = prev.node->next;
          prev.node->next = nw.index;
        }
        nw.node->c = word[m++];
        for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
          nw.node->str[k] = word[m++];
          nw.node->strlen++;
        }
        tuple = nw;
      }
      assert(n == rest);
      assert(m == len);

      /* mark */
      tuple.node->isdata = 1;
      tuple.node->data = data;

      return tuple;
    }

    inline
    Tuple _parent_child_append_tail(Tuple tuple, Tuple parent, uint16_t i,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple    nw;
      uint32_t m, n;

      /*dprintf(2, "IDOR X len:%u\n", len);*/

      for (m = i, n = 0; m < len;) {
        /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
        assert(n < rest);
        nw = stride[n++];
        nw.node->iskey = 1;
        nw.node->isused = 1;
        if (m > i)
          tuple.node->child = nw.index;
        else {
          if (parent.index) {
            nw.node->next = parent.node->child;
            parent.node->child = nw.index;
          } else {
            nw.node->next = root;
            root = nw.index;
          }
        }
        nw.node->c = word[m++];
        for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
          nw.node->str[k] = word[m++];
          nw.node->strlen++;
        }
        tuple = nw;
      }
      assert(n == rest);
      assert(m == len);

      /* mark */
      tuple.node->isdata = 1;
      tuple.node->data = data;

      return tuple;
    }

    inline
    Tuple _become_child_append_tail(Tuple tuple, uint16_t i,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple    nw;
      uint32_t m, n;

      /*printf(2, "IDOR R len:%u\n", len);*/

      for (m = i, n = 0; m < len;) {
        /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
        assert(n < rest);
        nw = stride[n++];
        nw.node->iskey = 1;
        nw.node->isused = 1;
        tuple.node->child = nw.index;
        nw.node->c = word[m++];
        for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
          nw.node->str[k] = word[m++];
          nw.node->strlen++;
        }
        tuple = nw;
      }
      assert(n == rest);
      assert(m == len);

      /* mark */
      tuple.node->isdata = 1;
      tuple.node->data = data;

      return tuple;
    }

    inline
    Tuple _append_tail_to_root(Tuple tuple,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple    nw;
      uint32_t m, n;

      /*dprintf(2, "IDOR M len:%u\n", len);*/

      for (m = 0, n = 0; m < len;) {
        /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
        assert(n < rest);
        nw = stride[n++];
        nw.node->iskey = 1;
        nw.node->isused = 1;
        if (m > 0)
          tuple.node->child = nw.index;
        else
          root = nw.index;
        nw.node->c = word[m++];
        for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
          nw.node->str[k] = word[m++];
          nw.node->strlen++;
        }
        tuple = nw;
      }
      assert(n == rest);
      assert(m == len);

      /* mark */
      tuple.node->isdata = 1;
      tuple.node->data = data;

      return tuple;
    }

    inline
    Tuple _split_0_set(Tuple tuple, uint16_t i,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple nw = stride[0];

      for (uint8_t k = 0; k < tuple.node->strlen; k++) {
        if (k == 0)
          nw.node->c = tuple.node->str[k];
        else
          nw.node->str[k - 1] = tuple.node->str[k];
      }
      nw.node->strlen = tuple.node->strlen - 1;
      nw.node->child = tuple.node->child;
      nw.node->isused = 1;
      nw.node->iskey = 1;
      tuple.node->child = nw.index;
      tuple.node->strlen = 0;
      tuple.node->isdata = 1;
      tuple.node->data = data;

      return tuple;
    }

    inline
    Tuple _split_n_child(Tuple tuple, uint16_t i, uint8_t n,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple dend = stride[0];

      dend.node->isused = 1;
      dend.node->iskey = 1;

      dend.node->c = tuple.node->str[n];
      dend.node->child = tuple.node->child;
      tuple.node->child = dend.index;

      dend.node->isdata = 1;
      dend.node->data = data;

      if (n + 1 < tuple.node->strlen) {
        Tuple nw = stride[1];
        nw.node->isused = 1;
        nw.node->iskey = 1;

        for (uint8_t k = 0; k < tuple.node->strlen - n - 1; k++) {
          if (k == 0)
            nw.node->c = tuple.node->str[n + k + 1];
          else {
            nw.node->str[k - 1] = tuple.node->str[n + k + 1];
            nw.node->strlen++;
          }
        }

        nw.node->child = dend.node->child;
        dend.node->child = nw.index;
      }
      tuple.node->strlen = n;

      return dend;
    }

    inline
    Tuple _split_n_next(Tuple tuple, uint16_t i, uint8_t n,
      uint16_t len, const K word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
    {
      Tuple m = stride[0];

      m.node->isused = 1;
      m.node->iskey = 1;

      for (uint8_t k = 0; k < tuple.node->strlen - n; k++) {
        if (k == 0)
          m.node->c = tuple.node->str[n + k];
        else {
          m.node->str[k - 1] = tuple.node->str[n + k];
          m.node->strlen++;
        }
      }

      m.node->child = tuple.node->child;
      tuple.node->child = m.index;
      tuple.node->strlen = n;

      if (word[i] < m.node->c) {
        tuple = _parent_child_append_tail(mktuple(0, 0),
            tuple, i, len, word, rest - 1, stride + 1, data);
      } else {
        tuple = _prev_child_append_tail(mktuple(0, 0),
            m, i, len, word, rest - 1, stride + 1, data);
      }

      return tuple;
    }


    inline
    Tuple _find_i(uint16_t len, const K word[/*len*/])
    {
      int       out = 0;
      uint32_t  i = 0;
      K   c = word[i];
      uint8_t   nlen;
      Tuple     tuple;
      TNodeIter iter = TNodeIter(nodes, 0);

      if (root) {
        tuple = iter.get_node(root);
        for (i = 0, c = word[i], out = 1; tuple.index && i < len; c = word[i]) {
          if (c == tuple.node->c) {
            i++;
            nlen = tuple.node->strlen;
            if (i == len) {
              if (tuple.node->isdata) {
                /* found substring */
                return tuple;
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
            tuple = iter.get_node(tuple.node->child);
          } else {
            if (tuple.node->next) {
              tuple = iter.get_node(tuple.node->next);
            } else {
              break;
            }
          }
        }
      }

      return mktuple(0, 0);
    }

    struct Iter {
      Trie    & trie;

      uint16_t  len;
      uint16_t  alen;
      K       * word;

      int32_t   spos;
      uint16_t  slen;
      uint16_t  aslen;
      Tuple   * stride;

      TNodeIter iter;
      Tuple     tuple;
      int       fsm; /* stop = 0, child = 1, next = 2 */

      Iter(Trie & trie) : trie(trie), iter(trie.nodes, 0)
      {
        len = alen = 0;
        word = 0;
        spos = 0;
        slen = aslen = 0;
        stride = 0;
        iter = TNodeIter(trie.nodes, 0);
        tuple = iter.get_node(trie.root);

        fsm = 1;
      }
      ~Iter()
      {
        if (word)
          delete[] word;
        if (stride)
          delete[] stride;
      }

      int go()
      {
        while (tuple.index) {
          if (fsm != 0) {
            slen = spos + 1;

            /* allocate */
            if (slen >= aslen) {
              uint16_t  oslen;
              oslen = aslen;
              aslen = ((slen) + 15L) & ~15L; /* align 16 */

              if (stride) {
                Tuple * tmp = new Tuple[aslen];
                memcpy(tmp, stride, sizeof(Tuple) * oslen);
                delete[] stride;
                stride = tmp;
              } else {
                stride = new Tuple[aslen];
              }
            }

            stride[spos] = tuple;

            if (tuple.node->isdata) {
              len = 0;
              for (int32_t k = 0; k < slen; k++) {
                if (stride[k].node->isdata)
                  len++;
                else
                  len = len + 1 + stride[k].node->strlen;
              }
              if (len > alen) {
                alen = ((len) + 15L) & ~15L; /* align 16 */
                if (word) {
                  K * tmp = new K[alen];
                  delete[] word;
                  word = tmp;
                } else {
                 word = new K[alen];
                }
              }
              for (int32_t k = 0, j = 0; k < slen; k++) {
                if (stride[k].node->isdata)
                  word[j++] = stride[k].node->c;
                else {
                  word[j++] = stride[k].node->c;
                  for (uint8_t n = 0; n < stride[k].node->strlen; n++) {
                    word[j++] = stride[k].node->str[n];
                  }
                }
              }

              fsm = 0;
              return 1;
            }
          }

          if (fsm == 0) {
            fsm = 1;
          }

          if (fsm == 1) {
            if (tuple.node->child) {
              tuple = iter.get_node(tuple.node->child); spos++;
              continue;
            } else {
              fsm = 2;
            }
          }

          if (fsm == 2) {
            /* rewind the stack */
            for (; spos >= 0 && !stride[spos].node->next; spos--);
            if (spos < 0)
              return 0;
            tuple = iter.get_node(stride[spos].node->next); fsm = 1;
          }
        }

        return 0;
      }
    };

    Error insert(uint16_t len, const K word[/*len*/], T data, bool rep)
    {
      if (!word)
        return NO_WORD;
      if (!len)
        return EMPTY_WORD;

      Epo epo = {SUCCESS, {0, 0}, {0, 0}, {0, 0}, ROOT, 0, 0};


      if (root) {
        epo.tuple = TNodeIter(nodes, 0).get_node(root);
        epo = _insert(epo, len, word, data, rep);
      } else {
        uint32_t  rest = _calc_stride_length(epo, len);
        Tuple stride[rest];
        if ((epo.err = _stride_alloc(rest, stride)))
          return FAILURE;
        epo.tuple = _append_tail_to_root(epo.tuple,
          len, word, rest, stride, data);
      }

      return epo.err;
    }

    Epo _insert(Epo epo, uint16_t len, const K word[/*len*/], T data, bool rep)
    {
      uint32_t  rest;
      TNodeIter iter = TNodeIter(nodes, 0);
      if (!epo.tuple.index)
        return epo;

      epo = _insert_decide(epo.tuple, iter, len, word, rep);
      if (epo.err)
        return epo;

      /* allocate space for the rest of our key
       * and roll back on failure... */

      rest = _calc_stride_length(epo, len);
      Tuple stride[rest];
      if (epo.act) {
        if ((epo.err = _stride_alloc(rest, stride)))
          return epo;
      }

      /* _everything_ should be fine now */

      switch (epo.act) {
        case PREV:
          epo.tuple = _prev_child_append_tail(epo.tuple,
            epo.prev, epo.i, len, word, rest, stride, data);
          break;
        case PARENT:
          epo.tuple = _parent_child_append_tail(epo.tuple,
            epo.parent, epo.i, len, word, rest, stride, data);
          break;
        case CHILD:
          epo.tuple = _become_child_append_tail(epo.tuple,
            epo.i, len, word, rest, stride, data);
          break;
        case SET:
          epo.tuple.node->isdata = 1;
          epo.tuple.node->data = data;
          break;
        case SPLIT_0_SET:
         epo.tuple =  _split_0_set(epo.tuple,
            epo.i, len, word, rest, stride, data);
          break;
        case SPLIT_N_CHILD:
          epo.tuple = _split_n_child(epo.tuple,
            epo.i, epo.n, len, word, rest, stride, data);
          break;
        case SPLIT_N_NEXT:
          epo.tuple = _split_n_next(epo.tuple,
            epo.i, epo.n, len, word, rest, stride, data);
          break;
        case ROOT:
        case FAIL:
          epo.err = FAILURE;
      }

      return epo;
    }

    Error remove(uint16_t len, const K word[/*len*/]);

    Error find(uint16_t len, const K word[/*len*/], T & data)
    {
      Tuple tuple;

      if (!word)
        return NO_WORD;
      if (!len)
        return EMPTY_WORD;

      tuple = _find_i(len, word);

      if (!tuple.index)
        return NOT_FOUND;

      data = tuple.node->data;

      return SUCCESS;
    }

    void print(int pipe)
    {
      FILE * fd = fdopen(pipe, "w");
      if (!fd)
        return;

      fprintf(fd, "digraph trie {\n");
      fprintf(fd, " graph [rankdir = TD]\n");
      fprintf(fd, " node [fontsize = 12, fontname = \"monospace\"]\n");
      fprintf(fd, " edge []\n");

      fprintf(fd,
          " \"trie\" [ shape = plaintext, label = <"
          "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">"
          "<tr><td bgcolor=\"red\">trie</td></tr>"
          "<tr><td port=\"f0\" bgcolor=\"gray\">%u</td></tr>"
          "</table>>]\n", root);

      if (root) {
        fprintf(fd, " \"trie\":f0 -> \"node%u\":f0;\n", root);
      }
      uint16_t  len;
      uint16_t  alen;
      K       * word;

      int32_t   spos;
      uint16_t  slen;
      uint16_t  aslen;
      Tuple   * stride;

      TNodeIter iter = TNodeIter(nodes, 0);
      Tuple     tuple = iter.get_node(root);
      int       fsm = 1; /* stop = 0, child = 1, next = 2 */

      len = alen = 0;
      word = 0;
      spos = 0;
      slen = aslen = 0;
      stride = 0;

      while (tuple.index) {
        slen = spos + 1;

        /* allocate */
        if (slen >= aslen) {
          uint16_t  oslen;
          oslen = aslen;
          aslen = ((slen) + 15L) & ~15L; /* align 16 */

          if (stride) {
            Tuple * tmp = new Tuple[aslen];
            memcpy(tmp, stride, sizeof(Tuple) * oslen);
            delete[] stride;
            stride = tmp;
          } else {
            stride = new Tuple[aslen];
          }
        }

        stride[spos] = tuple;

        if (tuple.node->isdata) {
          len = 0;
          for (int32_t k = 0; k < slen; k++) {
            if (stride[k].node->isdata)
              len++;
            else
              len = len + 1 + stride[k].node->strlen;
          }
          if (len > alen) {
            alen = ((len) + 15L) & ~15L; /* align 16 */
            if (word) {
              K * tmp = new K[alen];
              delete[] word;
              word = tmp;
            } else {
             word = new K[alen];
            }
          }
          for (int32_t k = 0, j = 0; k < slen; k++) {
            if (stride[k].node->isdata)
              word[j++] = stride[k].node->c;
            else {
              word[j++] = stride[k].node->c;
              for (uint8_t n = 0; n < stride[k].node->strlen; n++) {
                word[j++] = stride[k].node->str[n];
              }
            }
          }
        }

        {
          if (tuple.index) {
            fprintf(fd, " \"node%u\" [ shape = plaintext, label = <", tuple.index);
            fprintf(fd, "<table cellborder=\"1\" cellspacing=\"0\" cellpadding=\"0\" border=\"0\">");
            fprintf(fd, "<tr>");
            fprintf(fd, "<td colspan=\"3\" port=\"f0\">%u:%s%s:%d</td>",
                tuple.index, tuple.node->iskey ? "k" : "",
                tuple.node->isdata ? "d" : "", tuple.node->strlen);
            if (tuple.node->next)
              fprintf(fd, "<td port=\"f2\">→%u</td>", tuple.node->next);
            fprintf(fd, "</tr>");
            fprintf(fd, "<tr>");
            if (tuple.node->isdata)
              fprintf(fd, "<td bgcolor=\"black\" port=\"f4\"><font color=\"white\">↭</font></td>");
            fprintf(fd, "<td port=\"f1\" bgcolor=\"gray\">%c</td>", tuple.node->c);
            if (!tuple.node->isdata)
              fprintf(fd, "<td port=\"f4\" bgcolor=\"gray\">%.*s</td>",
                  tuple.node->strlen, tuple.node->str);
            if (tuple.node->child)
              fprintf(fd, "<td port=\"f3\">↓%u</td>", tuple.node->child);
            fprintf(fd, "</tr>");
            fprintf(fd, "</table>");
            fprintf(fd, ">]\n");

            if (tuple.node->child) {
              fprintf(fd, " \"node%u\":f3 -> \"node%u\":f0 [color=red];\n", tuple.index, tuple.node->child);
            }

            if (tuple.node->next) {
              fprintf(fd, " \"node%u\":f2 -> \"node%u\" [color=blue, minlen=0];\n", tuple.index, tuple.node->next);
            }
          }
        }

        if (fsm == 1) {
          if (tuple.node->child) {
            tuple = iter.get_node(tuple.node->child); spos++;
            continue;
          } else {
            fsm = 2;
          }
        }

        if (fsm == 2) {
          /* rewind the stack */
          for (; spos >= 0 && !stride[spos].node->next; spos--);
          if (spos < 0)
            break;
          tuple = iter.get_node(stride[spos].node->next); fsm = 1;
        }
      }

      if (word)
        delete[] word;
      if (stride)
        delete[] stride;

      fprintf(fd, "}\n");

      fclose(fd);
    }

    Error serialize(int pipe)
    {
      if (fcntl(pipe, F_GETFD) != -1) {
        char thdr[] = "trie 0.0\0\0\0\0\0\0";
        char tftr[] = "trie end";
        char bhdr[] = "bank 0.0";
        uint16_t n = N;
        write(pipe, thdr, 14);
        write(pipe, &n, sizeof(n));
        for (Bank * bank = nodes, * next = bank ? bank->next : 0; bank;
           bank = next, next = bank ? bank->next : 0) {
          write(pipe, bhdr, 8);
          write(pipe, &bank->start, sizeof(bank->start));
          write(pipe, &bank->length, sizeof(bank->length));
          for (uint32_t k = 0; k < bank->length; k++) {
            write(pipe, &bank->nodes[k], sizeof(TNode));
          }
        }
        write(pipe, &root, sizeof(root));
        write(pipe, &freelist, sizeof(freelist));
        write(pipe, tftr, 8);
      }

      return SUCCESS;
    }
  };
};
