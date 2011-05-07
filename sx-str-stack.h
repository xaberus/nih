struct sx_str_stack_chunk {
  struct sx_str_stack_chunk * next;
};

struct sx_str_stack_page {
  struct sx_str_stack_page * prev;
  unsigned int count;
  sx_str_t * data[16];
};

struct sx_str_stack {
  struct sx_str_stack_page * tail;

  struct sx_str_stack_chunk * free;
};

typedef struct sx_str_stack sx_str_stack_t;

static inline struct sx_str_stack * sx_str_stack_init(sx_str_stack_t * stack)
{
  stack->tail = malloc(sizeof(struct sx_str_stack_page));
  if (!stack->tail)
    return NULL;
  stack->tail->prev = NULL;
  stack->tail->count = 0;
  stack->free = NULL;

  return stack;
}

static inline sx_str_t ** sx_str_stack_pushn(sx_str_stack_t * stack)
{
  struct sx_str_stack_page * page = NULL;

  if (!stack->tail)
    return NULL;

  if (stack->tail->count < 16) {
    return &stack->tail->data[stack->tail->count++];
  }

  if (stack->free) {
    page = (struct sx_str_stack_page *) stack->free;
    stack->free = stack->free->next;
  }

  if (!page) {
    page = malloc(sizeof(struct sx_str_stack_page));
    if (!page)
      return NULL;
  }

  page->prev = stack->tail;
  page->count = 0;
  stack->tail = page;

  return &stack->tail->data[stack->tail->count++];
}

static inline sx_str_t ** sx_str_stack_push(sx_str_stack_t * stack, sx_str_t * data)
{
  sx_str_t ** d = NULL;

  d = sx_str_stack_pushn(stack);
  if (d)
    *d = data;

  return d;
}
static inline sx_str_t * sx_str_stack_top(sx_str_stack_t * stack)
{
  if (!stack->tail || !stack->tail->count)
    return NULL;

  return stack->tail->data[stack->tail->count - 1];
}

static inline void sx_str_stack_pop(sx_str_stack_t * stack)
{
  struct sx_str_stack_chunk * chunk = NULL;

  if (!stack->tail || !stack->tail->count)
    return;

  stack->tail->count--;

  if (stack->tail->prev) {
    if (!stack->tail->count) {
      chunk = (struct sx_str_stack_chunk *) stack->tail;
      stack->tail = stack->tail->prev;
    }
  }

  if (chunk) {
    chunk->next = stack->free;
    stack->free = chunk;
  }
}

static inline void sx_str_stack_clear(sx_str_stack_t * stack)
{
  {
    struct sx_str_stack_page * page = NULL;
    struct sx_str_stack_page * prev = NULL;
    for (
         page = stack->tail, prev = page ? page->prev : NULL;
         page;
         page = prev, prev = page ? page->prev : NULL
        ) {
      free(page);
    }
  }
  {
    struct sx_str_stack_chunk * chunk = NULL;
    struct sx_str_stack_chunk * next = NULL;
    for (
         chunk = stack->free, next = chunk ? chunk->next : NULL;
         chunk;
         chunk = next, next = chunk ? chunk->next : NULL
        ) {
      free(chunk);
    }
  }
  stack->tail = NULL;
  stack->free = NULL;
}
