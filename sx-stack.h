#include <stdlib.h>
#include "sx-pool.h"

struct sx_stack_chunk {
  struct sx_stack_chunk * next;
};
struct sx_stack_page {
  struct sx_stack_page * prev;
  unsigned int count;
  sx_t * data[128];
};
struct sx_stack {
  struct sx_stack_page * tail;
  struct sx_stack_chunk * free;
  sx_pool_t * allocator;
};
typedef struct sx_stack sx_stack_t;
static inline struct sx_stack * sx_stack_init(sx_pool_t * allocator, sx_stack_t * stack)
{
  stack->tail = sx_pool_getmem(allocator, sizeof(struct sx_stack_page));
  if (!stack->tail)
    return NULL;
  stack->tail->prev = NULL;
  stack->tail->count = 0;
  stack->free = NULL;
  stack->allocator = allocator;
  return stack;
}
static inline sx_t ** sx_stack_pushn(sx_stack_t * stack)
{
  struct sx_stack_page * page = NULL;

  if (!stack->tail)
    return NULL;
  if (stack->tail->count < 128) {
    return &stack->tail->data[stack->tail->count++];
  }
  if (stack->free) {
    page = (struct sx_stack_page *) stack->free;
    stack->free = stack->free->next;
  }
  if (!page) {
    page = sx_pool_getmem(stack->allocator, sizeof(struct sx_stack_page));
    if (!page)
      return NULL;
  }
  page->prev = stack->tail;
  page->count = 0;
  stack->tail = page;
  return &stack->tail->data[stack->tail->count++];
}
static inline sx_t ** sx_stack_push(sx_stack_t * stack, sx_t * data)
{
  sx_t ** d = NULL;

  d = sx_stack_pushn(stack);
  if (d)
    *d = data;
  return d;
}
static inline sx_t * sx_stack_top(sx_stack_t * stack)
{
  if (!stack->tail || !stack->tail->count)
    return NULL;
  return stack->tail->data[stack->tail->count - 1];
}
static inline void sx_stack_pop(sx_stack_t * stack)
{
  struct sx_stack_chunk * chunk = NULL;

  if (!stack->tail || !stack->tail->count)
    return;
  stack->tail->count--;
  if (stack->tail->prev) {
    if (!stack->tail->count) {
      chunk = (struct sx_stack_chunk *) stack->tail;
      stack->tail = stack->tail->prev;
    }
  }
  if (chunk) {
    chunk->next = stack->free;
    stack->free = chunk;
  }
}
static inline void sx_stack_clear(sx_stack_t * stack)
{
  {
    struct sx_stack_page * page = NULL;
    struct sx_stack_page * prev = NULL;
    for (
         page = stack->tail, prev = page ? page->prev : NULL;
         page;
         page = prev, prev = page ? page->prev : NULL
        ) {
      sx_pool_retmem(stack->allocator, page);
    }
  }
  {
    struct sx_stack_chunk * chunk = NULL;
    struct sx_stack_chunk * next = NULL;
    for (
         chunk = stack->free, next = chunk ? chunk->next : NULL;
         chunk;
         chunk = next, next = chunk ? chunk->next : NULL
        ) {
      sx_pool_retmem(stack->allocator, chunk);
    }
  }
  stack->tail = NULL;
  stack->free = NULL;
}
