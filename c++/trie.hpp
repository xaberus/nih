#include <stdint.h>
#include <assert.h>

namespace Util {
  template<typename T, unsigned N = 1024>
  struct Trie {
    enum Action {
      FAILURE = 0,
      PREV,
      PARENT,
      CHILD,
      ROOT,
      SET,
      SPLIT_0_SET,
      SPLIT_N_CHILD,
      SPLIT_N_NEXT,
    };

    enum Error {
      SUCCESS = 0,
      FALILURE = 0,
      ALLOCATION_FALILURE = 0,
      EMPTY_WORD,
      NO_WORD,
      DUPLICATE,
      CORRUPTION,
      NOT_FOUND,
    };

    struct Tnode {
      uint8_t c;
      __extension__ struct {
        unsigned int iskey : 1;
        unsigned int isdata : 1;
        unsigned int isused : 1;
        unsigned int strlen : 4;
        unsigned int _reserved : 1;
      };
      unsigned next : 24;
      unsigned child : 24;
      __extension__ union {
        uint8_t str[8];
        T data;
      };
    };

    struct Tuple {
      Tnode * node;
      uint32_t index;
    };

    static Tuple mktuple(Tnode * node, uint32_t index)
    {
      Tuple tuple = {node, index};

      return tuple;
    }

    struct Bank {
      uint32_t start;
      uint32_t length;

      Bank * prev;
      Bank * next;

      Tnode nodes[N];

      Bank(uint32_t start) : start(start), length(0), prev(0), next(0)
      {
        Tnode e = {0, {0, 0, 0, 0, 0}, 0, 0, {{0}}};

        for (uint32_t k = 0, len = N; k < len; k++)
          nodes[k] = e;
      }

      Tuple mknode()
      {
        if (length < N) {
          uint32_t index = idx2index(start + length);
          Tnode * node = &nodes[length];

          length++;
          return mktuple(node, index);
        }

        return mktuple(0, 0);
      }
    };

    struct TnodeIter {
      Bank * bank;
      uint32_t idx;
      TnodeIter(Bank * bank, uint32_t idx) : bank(bank), idx(idx)
      {
      }

      Bank * get_bank()
      {
        register Bank   * b = bank;
        register uint32_t i = idx;
        register uint32_t bs = b->start;
        register uint32_t be = bs+N;

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
        Tnode * node;
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

    Tuple  mknode()
    {
      Bank * bank;

      if (freelist) { /* reuse nodes from freelist */
        TnodeIter iter = TnodeIter(nodes, index2idx(freelist));
        Tnode   * node;

        if ((bank = iter.get_bank())) {
          /* get */
          node = &bank->nodes[iter.idx - bank->start];
          if (!node->isused) {
            freelist = node->next;
            return mktuple(node, idx2index(iter.idx));
          }
        }
      } else {
        Tuple tuple = abank->mknode();

        if (!tuple.index && abank) {
          uint32_t start = abank->start + N;

          bank = new Bank(start);

          tuple = bank->mknode();

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
      TnodeIter iter = TnodeIter(nodes, 0);
      Tuple     tuple = iter.get_node(index);

      tuple.node->next = freelist;

      tuple.node->iskey = 0;
      tuple.node->isdata = 0;
      tuple.node->isused = 0;
      tuple.node->strlen = 0;
      tuple.node->next = 0;
      tuple.node->child = 0;

      freelist = tuple.index;
    }

    inline
    Epo _insert_decide(Tuple tuple, struct TnodeIter & iter,
      uint16_t len, const uint8_t word[/*len*/], bool rep)
    {
      Tuple    prev = {0, 0};
      Tuple    parent = {0, 0};
      Tuple    tmp = {0, 0};
      Error    err = SUCCESS;
      Action   act = FAILURE;
      uint32_t i = 0;
      uint8_t  n = 0;
      uint8_t  c;
      uint8_t  nlen;

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

      if (!act)
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
        nw = mknode();
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data)
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
    Tuple _find_i(uint16_t len, const uint8_t word[/*len*/])
    {
      int       out = 0;
      uint32_t  i = 0;
      uint8_t   c = word[i];
      uint8_t   nlen;
      Tuple     tuple;
      TnodeIter iter = TnodeIter(nodes, 0);

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

    Error insert(uint16_t len, const uint8_t word[/*len*/], T data, bool rep)
    {
      uint32_t  rest;
      TnodeIter iter = TnodeIter(nodes, 0);
      Epo       epo = {SUCCESS, {0, 0}, {0, 0}, {0, 0}, ROOT, 0, 0};

      if (!word)
        return NO_WORD;
      if (!len)
        return EMPTY_WORD;

      if (root) {
        epo.tuple = iter.get_node(root);

        epo = _insert_decide(epo.tuple, iter, len, word, rep);
        if (epo.err)
          return epo.err;
      }

      /* allocate space for the rest of our key
       * and roll back on failure... */

      rest = _calc_stride_length(epo, len);;
      Tuple stride[rest];
      if (epo.act) {
        if ((epo.err = _stride_alloc(rest, stride)))
          return epo.err;
      }

      /* _everything_ should be fine now */

      switch (epo.act) {
        case PREV:
          _prev_child_append_tail(epo.tuple,
            epo.prev, epo.i, len, word, rest, stride, data);
          break;
        case PARENT:
          _parent_child_append_tail(epo.tuple,
            epo.parent, epo.i, len, word, rest, stride, data);
          break;
        case CHILD:
          _become_child_append_tail(epo.tuple,
            epo.i, len, word, rest, stride, data);
          break;
        case ROOT:
          _append_tail_to_root(epo.tuple,
            len, word, rest, stride, data);
          break;
        case SET:
          epo.tuple.node->isdata = 1;
          epo.tuple.node->data = data;
          break;
        case SPLIT_0_SET:
          _split_0_set(epo.tuple,
            epo.i, len, word, rest, stride, data);
          break;
        case SPLIT_N_CHILD:
          _split_n_child(epo.tuple,
            epo.i, epo.n, len, word, rest, stride, data);
          break;
        case SPLIT_N_NEXT:
          _split_n_next(epo.tuple,
            epo.i, epo.n, len, word, rest, stride, data);
          break;
        case FAILURE:
          return FALILURE;
      }

      return SUCCESS;
    }

    Error remove(uint16_t len, const uint8_t word[/*len*/]);

    Error find(uint16_t len, const uint8_t word[/*len*/], T & data)
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
  };
};
