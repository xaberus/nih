
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

