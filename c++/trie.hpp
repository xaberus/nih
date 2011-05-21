
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
      uint32_t end;
      uint32_t length;

      Bank * prev;
      Bank * next;

      Tnode nodes[N];

      Bank(uint32_t start, uint32_t end);
      Tuple mknode();
    };

    struct TnodeIter {
      Bank * bank;
      uint32_t idx;
      TnodeIter(Bank * bank, uint32_t idx) : bank(bank), idx(idx) {}
      Bank * get_bank();
      Tuple  get_node(uint32_t index);
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

    Trie();
    ~Trie();

    Error insert(uint16_t len, const uint8_t word[/*len*/], T data, bool rep);
    Error remove(uint16_t len, const uint8_t word[/*len*/]);
    Error find(uint16_t len, const uint8_t word[/*len*/], T & data);

    Tuple _find_i(uint16_t len, const uint8_t word[/*len*/]);

    Tuple  mknode();
    void   rmnode(uint32_t index);

    Epo _insert_decide(Tuple tuple, struct TnodeIter & iter,
      uint16_t len, const uint8_t word[/*len*/], bool rep);

    uint16_t _calc_stride_length(Epo & epo, uint16_t len);
    Error _stride_alloc(uint16_t rest, Tuple stride[/*rest*/]);

    Tuple _prev_child_append_tail(Tuple tuple, Tuple prev, uint16_t i,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _parent_child_append_tail(Tuple tuple, Tuple parent, uint16_t i,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _become_child_append_tail(Tuple tuple, uint16_t i,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _append_tail_to_root(Tuple tuple,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _split_0_set(Tuple tuple, uint16_t i,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _split_n_child(Tuple tuple, uint16_t i, uint8_t n,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
    Tuple _split_n_next(Tuple tuple, uint16_t i, uint8_t n,
      uint16_t len, const uint8_t word[/*len*/], uint16_t rest, Tuple stride[/*rest*/], T data);
  };
};
