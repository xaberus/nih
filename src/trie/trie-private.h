void               trie_print(trie_t * trie, int fd);
struct tnode_tuple trie_find_i(trie_t * trie, uint16_t len, const uint8_t word[len]);

#if 0
# include <assert.h>
# define trie_assert(__arg) assert(__arg)
#else
# define trie_assert(__arg)
#endif

#define linked_list_safe_foreach(__type, __iter, __init) \
  for (__type * __iter = (__init), * __next = __iter ? __iter->next : NULL; __iter; \
       __iter = __next, __next = __iter ? __iter->next : NULL)

struct tnode_tuple trie_mknode(trie_t * trie);
void               trie_remnode(trie_t * trie, uint32_t index);

inline static
struct tnode_tuple tnode_tuple(tnode_t * node, uint32_t index)
{
  struct tnode_tuple ret = {
    .node = node,
    .index = index,
  };

  return ret;
}

inline static
uint32_t tnode_bank_size(uint16_t addrbits)
{
  return (1 << (32 - addrbits)); /* 2^(32 - 12) */
}

inline static
tbank_t * tnode_bank_alloc(const mem_allocator_t * a, uint32_t size, uint32_t addr)
{
  tbank_t * bank;

  bank = mem_alloc(a, sizeof(tbank_t) + sizeof(tnode_t) * size);

  if (bank) {
    memset(bank, 0, sizeof(tbank_t) + sizeof(tnode_t) * size);
    bank->used = 0;
    bank->addr = addr;
  }

  return bank;
}

inline static
struct tnode_tuple tnode_bank_mknode(tbank_t * bank, uint32_t size, uint16_t nodebits)
{
  if (!bank)
    return tnode_tuple(NULL, 0);

  uint32_t next = bank->used;

  if (next < size) {
    tnode_t * node = &bank->nodes[next];
    memset(node, 0, sizeof(tnode_t));
    bank->used++;
    return tnode_tuple(node, IDX2INDEX(next | (bank->addr << nodebits)));
  }

  return tnode_tuple(NULL, 0);
}

inline static
struct tnode_tuple tnode_get(trie_t * trie, uint32_t index)
{
  tbank_t * bank;
  tnode_t * node;

  index = INDEX2IDX(index);

  uint32_t  addr = (index & trie->addrmask) >> trie->nodebits;
  uint32_t  idx  = (index & trie->nodemask);

  if (addr < trie->banks && idx < trie->banksize) {
    bank = trie->nodes[addr];
    node = &bank->nodes[idx];
    if (node->isused) {
      return (struct tnode_tuple) {
        .node = node,
        .index = IDX2INDEX(index),
      };
    }
  }

  return (struct tnode_tuple){
    .node = NULL,
    .index = 0,
  };
}

inline static
err_t _trie_stride_alloc(trie_t * trie, uint16_t rest, struct tnode_tuple stride[rest])
{
  struct tnode_tuple new;

  for (uint32_t n = 0; n < rest; n++) {
    new = trie_mknode(trie);
    if (!new.index) {
      for (uint32_t k = 0; k < n; k++) {
        stride[k].node->isused = 1;
        trie_remnode(trie, stride[k].index);
      }
      return ERR_MEM_USE_ALLOC;
    }
    stride[n] = new;
  }
  return 0;
}

/* append child, then tail */
static inline
struct tnode_tuple __trie_prev_append_child_append_tail(trie_t * trie,
    struct tnode_tuple tuple,
    struct tnode_tuple prev,
    uint16_t i,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple new;
  uint32_t           m, n;

  /*dprintf(2, "IDOR T len:%u\n", len);*/

  (void) trie;
  /* append child, then tail */

  for (m = i, n = 0; m < len;) {
    /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
    trie_assert(n < rest);
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    if (m > i)
      tuple.node->child = new.index;
    else {
      new.node->next = prev.node->next;
      prev.node->next = new.index;
    }
    new.node->c = word[m++];
    for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
      new.node->str[k] = word[m++];
      new.node->strlen++;
    }
    tuple = new;
  }
  trie_assert(n == rest);
  trie_assert(m == len);

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* append child, then tail */
static inline
struct tnode_tuple __trie_parent_append_child_append_tail(trie_t * trie,
    struct tnode_tuple tuple,
    struct tnode_tuple parent,
    uint16_t i,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple new;
  uint32_t           m, n;

  /*dprintf(2, "IDOR X len:%u\n", len);*/

  (void) trie;

  for (m = i, n = 0; m < len;) {
    /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
    trie_assert(n < rest);
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    if (m > i)
      tuple.node->child = new.index;
    else {
      if (parent.index) {
        new.node->next = parent.node->child;
        parent.node->child = new.index;
      } else {
        new.node->next = trie->root;
        trie->root = new.index;
      }
    }
    new.node->c = word[m++];
    for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
      new.node->str[k] = word[m++];
      new.node->strlen++;
    }
    tuple = new;
  }
  trie_assert(n == rest);
  trie_assert(m == len);

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* become child, then append tail */
static inline
struct tnode_tuple __trie_become_child_append_tail(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t i,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple new;
  uint32_t           m, n;

  /*printf(2, "IDOR R len:%u\n", len);*/

  (void) trie;

  for (m = i, n = 0; m < len;) {
    /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
    trie_assert(n < rest);
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    tuple.node->child = new.index;
    new.node->c = word[m++];
    for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
      new.node->str[k] = word[m++];
      new.node->strlen++;
    }
    tuple = new;
  }
  trie_assert(n == rest);
  trie_assert(m == len);

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* append tail to empty root */
static inline
struct tnode_tuple __trie_append_tail_to_root(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple new;
  uint32_t           m, n;

  /*dprintf(2, "IDOR M len:%u\n", len);*/

  for (m = 0, n = 0; m < len;) {
    /*dprintf(2, "I m:%u, n: %u\n", m, n);*/
    trie_assert(n < rest);
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    if (m > 0)
      tuple.node->child = new.index;
    else
      trie->root = new.index;
    new.node->c = word[m++];
    for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
      new.node->str[k] = word[m++];
      new.node->strlen++;
    }
    tuple = new;
  }
  trie_assert(n == rest);
  trie_assert(m == len);

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

static inline
struct tnode_tuple __trie_split_0_set(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t i,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple new = stride[0];

  (void) trie;
  (void) word;
  (void) i;

  for (uint8_t k = 0; k < tuple.node->strlen; k++) {
    if (k == 0)
      new.node->c = tuple.node->str[k];
    else
      new.node->str[k - 1] = tuple.node->str[k];
  }
  new.node->strlen = tuple.node->strlen - 1;
  new.node->child = tuple.node->child;
  new.node->isused = 1;
  new.node->iskey = 1;
  tuple.node->child = new.index;
  tuple.node->strlen = 0;
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

static inline
struct tnode_tuple __trie_split_n_child(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t i,
    uint8_t n,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple dend = stride[0];

  (void) trie;
  (void) word;
  (void) i;

  dend.node->isused = 1;
  dend.node->iskey = 1;

  dend.node->c = tuple.node->str[n];
  dend.node->child = tuple.node->child;
  tuple.node->child = dend.index;

  dend.node->isdata = 1;
  dend.node->data = data;

  if (n + 1 < tuple.node->strlen) {
    struct tnode_tuple new = stride[1];
    new.node->isused = 1;
    new.node->iskey = 1;

    for (uint8_t k = 0; k < tuple.node->strlen - n - 1; k++) {
      if (k == 0)
        new.node->c = tuple.node->str[n + k + 1];
      else {
        new.node->str[k - 1] = tuple.node->str[n + k + 1];
        new.node->strlen++;
      }
    }

    new.node->child = dend.node->child;
    dend.node->child = new.index;
  }
  tuple.node->strlen = n;

  return dend;
}
static inline
struct tnode_tuple __trie_split_n_next(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t i,
    uint8_t n,
    uint16_t len,
    const uint8_t word[len],
    uint16_t rest,
    struct tnode_tuple stride[rest],
    uint64_t data)
{
  struct tnode_tuple m = stride[0];

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
    tuple = __trie_parent_append_child_append_tail(trie, tnode_tuple(NULL, 0),
        tuple, i, len, word, rest - 1, stride + 1, data);
  } else {
    tuple = __trie_prev_append_child_append_tail(trie, tnode_tuple(NULL, 0),
        m, i, len, word, rest - 1, stride + 1, data);
  }

  return tuple;
}

enum trie_insert_action {
  TRIE_INSERT_FAILURE = 0,
  TRIE_INSERT_PREV,
  TRIE_INSERT_PARENT,
  TRIE_INSERT_CHILD,
  TRIE_INSERT_ROOT,
  TRIE_INSERT_SET,
  TRIE_INSERT_SPLIT_0_SET,
  TRIE_INSERT_SPLIT_N_CHILD,
  TRIE_INSERT_SPLIT_N_NEXT,
};

struct trie_eppoit {
  err_t err;
  struct tnode_tuple parent;
  struct tnode_tuple prev;
  enum trie_insert_action act;
  uint16_t i;
  uint8_t n;
  struct tnode_tuple tuple;
};

inline static
uint32_t trie_calc_stride_length(struct trie_eppoit * epo, uint32_t len)
{
  uint32_t rest = len - epo->i;

  uint32_t m;

  for (m = epo->i, rest = 0; m < len;) {
    /*dprintf(2, "T m:%u, rest: %u\rest", m, rest);*/
    rest++;
    m++;
    for (uint8_t k = 0; k < 8 && m + 1 < len; k++) {
      m++;
    }
  }

  if (epo->act == TRIE_INSERT_SPLIT_N_CHILD) {
    if (epo->n + 1 < epo->tuple.node->strlen)
      rest = 2;
    else
      rest = 1;
  } else if (epo->act == TRIE_INSERT_SPLIT_0_SET) {
    rest = 1;
  } else if (epo->act == TRIE_INSERT_SPLIT_N_NEXT) {
    rest++;
  }

  /*dprintf(2, "CLEN: act: %d, len: %u, i: %u, delta: %u => %d\n",
    epo->act, len, epo->i, len - epo->i, rest);*/

  return rest;
}

inline static
struct trie_eppoit _trie_insert_decide(trie_t * trie,
    struct tnode_tuple tuple,
    uint16_t len,
    const uint8_t word[len],
    bool rep)
{
  (void) trie;
  struct tnode_tuple prev = {NULL, 0};
  struct tnode_tuple parent = {NULL, 0};
  struct tnode_tuple tmp = {NULL, 0};

  uint32_t           i, n = 0;
  uint8_t            c;
  uint8_t            nlen;
  int                act = TRIE_INSERT_FAILURE;
  err_t              err = 0;

  for (i = 0, c = word[i]; tuple.index && i < len; c = word[i]) {
    if (c == tuple.node->c) {
      i++;
      nlen = tuple.node->strlen;
      if (i == len) {
        if (!rep && tuple.node->isdata) { /* duplicate */
          err = ERR_DUPLICATE;
          break;
        } else {
          if (nlen > 0) {
            /* action: detach string, set data */
            act = TRIE_INSERT_SPLIT_0_SET;
          } else {
            /* action: set data */
            act = TRIE_INSERT_SET;
          }
          break;
        }
      } else if (!tuple.node->isdata && nlen > 0) {
        /* node has a string */
        for (n = 0; n < nlen && i < len; n++, i++) {
          if (word[i] != tuple.node->str[n]) {
            act = TRIE_INSERT_SPLIT_N_NEXT;
            goto break_main;
          } else if (i + 1 == len) {
            act = TRIE_INSERT_SPLIT_N_CHILD;
            goto break_main;
          }
        }
      }
      tmp = tnode_get(trie, tuple.node->child);
      if (tmp.index) {
        prev = tnode_tuple(NULL, 0);
        parent = tuple;
        tuple = tmp;
        continue;
      } else {
        /* if we end up here, it means, that we already have a key,
         * which is a prefix of our new key, so we use the free child field!
         * action: become child */
        act = TRIE_INSERT_CHILD;
        break;
      }
    } else if (c < tuple.node->c) {
      if (prev.index) {
        /* action: append child */
        act = TRIE_INSERT_PREV;
        break;
      } else {
        /* action: append child to parent */
        act = TRIE_INSERT_PARENT;
        break;
      }
    } else {
      if (tuple.node->next) {
        /* advance one child */
        prev = tuple;
        tuple = tnode_get(trie, tuple.node->next);
      } else {
        /* we reached the last child, action: append child */
        prev = tuple;
        act = TRIE_INSERT_PREV;
        break;
      }
    }
  }
break_main:

  if (!act)
    err = ERR_CORRUPTION;

  struct trie_eppoit ret = {
    .err = err,
    .parent = parent,
    .prev = prev,
    .act = act,
    .i = i,
    .n = n,
    .tuple = tuple,
  };
  return ret;
}
