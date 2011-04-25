
struct tnode_tuple trie_mknode(trie_t * trie);
void trie_remnode(trie_t * trie, uint32_t index);

inline static
struct tnode_tuple  tnode_tuple(struct tnode * node, uint32_t index)
{
  struct tnode_tuple ret = {
    .node = node,
    .index = index,
  };

  return ret;
}

inline static
struct tnode_iter   tnode_iter(struct tnode_bank * bank, uint32_t idx)
{
  struct tnode_iter ret = {
    .bank = bank,
    .idx = idx,
  };

  return ret;
}

inline static
struct tnode_bank * tnode_bank_alloc(uint32_t start, uint32_t end, const mem_allocator_t * a)
{
  uint32_t            size = (end - start);
  struct tnode_bank * bank;

  bank = mem_alloc(a, sizeof(struct tnode_bank) + sizeof(struct tnode) * size);

  if (bank) {
    memset(bank, 0, sizeof(struct tnode_bank) + sizeof(struct tnode) * size);

    bank->start = start;
    bank->end = end;
  }

  return bank;
}

inline static
struct tnode_tuple tnode_bank_mknode(struct tnode_bank * bank)
{
  if (!bank)
    return tnode_tuple(NULL, 0);

  uint32_t next = bank->start + bank->length;

  if (next < bank->end) {
    struct tnode * node = &bank->nodes[bank->length];

    memset(node, 0, sizeof(struct tnode));

    bank->length++;

    return tnode_tuple(node, IDX2INDEX(next));
  }

  return tnode_tuple(NULL, 0);
}

inline static
struct tnode_bank * tnode_iter_get_bank(struct tnode_iter * iter)
{
  struct tnode_bank * bank = iter->bank;
  uint32_t            idx = iter->idx;

  /* rewind */
  while (bank && (idx < bank->start || idx >= bank->end)) {
    /* note: the direction of the list is inversed! */
    if (idx < bank->start)
      bank = bank->next;
    else if (idx >= bank->end)
      bank = bank->prev;
  }

  if (bank)
    return (iter->bank = bank);

  return NULL;
}

inline static
struct tnode_tuple tnode_iter_get(struct tnode_iter * iter, uint32_t index)
{
  struct tnode_bank * bank;
  struct tnode      * node;

  if (!index || !iter)
    return tnode_tuple(NULL, 0);

  iter->idx = INDEX2IDX(index);

  if ((bank = tnode_iter_get_bank(iter))) {
    /* get */
    node = &bank->nodes[iter->idx - bank->start];
    if (node->isused)
      return tnode_tuple(node, index);
  }

  return tnode_tuple(NULL, 0);
}

static inline
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
struct tnode_tuple _trie_prev_append_child_append_tail
(
  trie_t * trie,
  struct tnode_tuple tuple,
  struct tnode_tuple prev,
  uint16_t i,
  uint16_t len, const uint8_t word[len],
  uint16_t rest, struct tnode_tuple stride[rest],
  uint64_t data)
{
  struct tnode_tuple new;
  (void) trie;
  /* append child, then tail */

  for (uint32_t m = i, n = 0; m < len; m++) {
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    new.node->c = word[m];
    if (m > i)
      tuple.node->child = new.index;
    else {
      new.node->next = prev.node->next;
      prev.node->next = new.index;
    }

    tuple = new;
  }

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* append child, then tail */
static inline
struct tnode_tuple _trie_parent_append_child_append_tail
(
  trie_t * trie,
  struct tnode_tuple tuple,
  struct tnode_tuple parent,
  uint16_t i,
  uint16_t len, const uint8_t word[len],
  uint16_t rest, struct tnode_tuple stride[rest],
  uint64_t data)
{
  struct tnode_tuple new;
  (void) trie;

  for (uint32_t m = i, n = 0; m < len; m++) {
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    new.node->c = word[m];
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

    tuple = new;
  }

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* become child, then append tail */
static inline
struct tnode_tuple _trie_become_child_append_tail
(
  trie_t * trie,
  struct tnode_tuple tuple,
  uint16_t i,
  uint16_t len, const uint8_t word[len],
  uint16_t rest, struct tnode_tuple stride[rest],
  uint64_t data)
{
  struct tnode_tuple new;
  (void) trie;

  for (uint32_t m = i, n = 0; m < len; m++) {
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    new.node->c = word[m];
    tuple.node->child = new.index;

    tuple = new;
  }

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

/* append tail to empty root */
static inline
struct tnode_tuple _trie_append_tail_to_root
(
  trie_t * trie,
  struct tnode_tuple tuple,
  uint16_t len, const uint8_t word[len],
  uint16_t rest, struct tnode_tuple stride[rest],
  uint64_t data)
{
  struct tnode_tuple new;

  for (uint32_t m = 0, n = 0; m < len; m++) {
    new = stride[n++];
    new.node->iskey = 1;
    new.node->isused = 1;
    new.node->c = word[m];
    if (m > 0)
      tuple.node->child = new.index;
    else
      trie->root = new.index;

    tuple = new;
  }

  /* mark */
  tuple.node->isdata = 1;
  tuple.node->data = data;

  return tuple;
}

enum trie_insert_action {
  TRIE_INSERT_FAILURE = 0,
  TRIE_INSERT_PREV,
  TRIE_INSERT_PARENT,
  TRIE_INSERT_CHILD,
  TRIE_INSERT_ROOT,
  TRIE_INSERT_SET,
};

struct trie_eppoit {
  err_t err;
  struct tnode_tuple parent;
  struct tnode_tuple prev;
  enum trie_insert_action act;
  uint16_t i;
  struct tnode_tuple tuple;
};

inline static
struct trie_eppoit _trie_insert_decide
(
  trie_t * trie,
  struct tnode_tuple tuple,
  struct tnode_iter * iter,
  uint16_t len, const uint8_t word[len],
  bool rep
)
{
  (void) trie;
  struct tnode_tuple prev = {NULL, 0};
  struct tnode_tuple parent = {NULL, 0};
  struct tnode_tuple tmp = {NULL, 0};

  uint32_t           i;
  uint8_t            c;
  int act = TRIE_INSERT_FAILURE;
  err_t err = 0;

  for (i = 0, c = word[i]; tuple.index && i < len; c = word[i]) {
    if (c == tuple.node->c) {
      i++;
      if (i == len) {
        if (!rep && tuple.node->isdata) { /* duplicate */
           err = ERR_DUPLICATE;
           break;
        } else {
          /* action: set data */
          act = TRIE_INSERT_SET;
          break;
        }
      }
      tmp = tnode_iter_get(iter, tuple.node->child);
      if (tmp.index) {
        prev = tnode_tuple(NULL, 0);
        parent = tuple;
        tuple = tmp;
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
        tuple = tnode_iter_get(iter, tuple.node->next);
      } else {
        /* we reached the last child, action: append child */
        prev = tuple;
        act = TRIE_INSERT_PREV;
        break;
      }
    }
  }

  if (!act)
    err = ERR_CORRUPTION;

  struct trie_eppoit ret = {
    .err = err,
    .parent = parent,
    .prev = prev,
    .act = act,
    .i = i,
    .tuple = tuple,
  };
  return ret;
}


void trie_print(trie_t * trie, int fd);
struct tnode_tuple trie_find_i(trie_t * trie, uint16_t len, const uint8_t word[len]);
