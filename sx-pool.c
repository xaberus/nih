#include <stdint.h>
#include <string.h>
#include "sx-pool.h"

/*#define VERBOSETEST*/

void sx_pool_init(sx_pool_t * pool)
{
  pool->head = pool->tail = NULL;
  pool->counter = 0;

  for (unsigned int i = 0; i < SX_POOL_FCACHE_UBITS_MAX; i++) {
    memset(&pool->fcache[i], 0, sizeof(struct cache_array));
  }
}

void sx_pool_clear(sx_pool_t * pool)
{
  struct chunk_header * c;
  struct chunk_header * prev;

  for (c = pool->tail, prev = c ? c->prev : NULL;
       c; c = prev, prev = c ? c->prev : NULL) {
    free(c);
  }

  sx_pool_init(pool);
}


#define ALIGN4096(_size) (((_size) + 4095L) & ~4095L)
#define ALIGN16(_size) (((_size) + 15L) & ~15L)

struct chunk_header * sx_pool_chunk_allocate(sx_pool_t * pool, size_t size)
{
  if (!pool || !size)
    return NULL;

  void                * p;
  struct chunk_header * h;
  struct data_header  * d;

  size = ALIGN4096(size);

  p = malloc(size);
  if (!p)
    return NULL;

  h = p;
  h->prev = NULL;
  h->size = size - sizeof(struct chunk_header);

  d = (struct data_header *) ((char *) p + sizeof(struct chunk_header));
  d->size = h->size - sizeof(struct data_header);
  d->flags = SX_POOL_DATA_FLAG_FREE;

  return h;
}

static inline
size_t sx_pool_next_power_of_two(size_t s)
{
  size_t i;

  if (s == 0)
    return 1;

  --s;

  for (i = 1; i < sizeof(size_t) * 8; i <<= 1)
    s |= s >> i;

  return s + 1;
}

static inline
int sx_pool_is_power_of_two(size_t s)
{
  return (s & (s - 1)) == 0;
}

static inline
unsigned int sx_pool_get_power_of_two(size_t s)
{
  unsigned int p;

  if (s == 0 || s == 1)
    return 0;

  for (p = 0; s; s >>= 1)
    p++;

  return p - 1;
}


static inline
struct data_header * sx_chunk_first_data(struct chunk_header * chunk)
{
  char * pos = (char *) chunk + sizeof(struct chunk_header);
  char * end = pos + chunk->size;

  if (pos < end && (size_t) (end - pos) > sizeof(struct data_header))
    return (struct data_header *) pos;

  return NULL;
}

static inline
struct data_header * sx_chunk_next_data(struct chunk_header * chunk, struct data_header * data)
{
  char * start = (char *) chunk + sizeof(struct chunk_header);
  char * end = start + chunk->size;
  char * pos = (char *) data + sizeof(struct data_header) + data->size;

  if (pos < end && (size_t) (end - pos) > sizeof(struct data_header))
    return (struct data_header *) pos;

  return NULL;
}

static inline
int sx_data_splitable(struct data_header * data, size_t size)
{
  if (data->size > sizeof(struct data_header) + size)
    return 1;

  return 0;
}

static inline
struct data_header * sx_chunk_data_split(struct chunk_header * chunk,
    struct data_header * data,
    size_t size)
{
  size = ALIGN16(size);

  char * start = (char *) chunk + sizeof(struct chunk_header);
  char * end = start + chunk->size;

  char * part_a = (char *) data;
  size_t part_a_size = size;
  char * part_b = (char *) data + sizeof(struct data_header) + size;
  size_t part_b_size = data->size - size - sizeof(struct data_header);

  if (part_a + sizeof(struct data_header) + part_a_size < end) {
    if (part_b + sizeof(struct data_header) + part_b_size <= end) {
      if (part_b_size > sizeof(struct data_header)) {
        struct data_header * a = (struct data_header *) part_a;
        struct data_header * b = (struct data_header *) part_b;
        a->size = part_a_size;
        a->flags = SX_POOL_DATA_FLAG_FREE;
        b->size = part_b_size;
        b->flags = SX_POOL_DATA_FLAG_FREE;
        return a;
      }
    }
  }

  return data;
}

static inline
void * sx_pool_data_to_pointer(struct data_header * data)
{
  if (data)
    return (char *) data + sizeof(struct data_header);

  return NULL;
}

#ifdef TEST
# include <bt.h>
static inline
void sx_pool_print(sx_pool_t * pool)
{
  struct chunk_header * c;
  struct data_header  * d;
  int                   count;

  bt_log("{\n");
  for (c = pool->tail; c; c = c->prev) {
    bt_log(" (s: %zu)", c->size);
    count = 0;
    for (d = sx_chunk_first_data(c); d; d = sx_chunk_next_data(c, d)) {
      if ((count) % 8 == 0)
        bt_log("\n  ");
      bt_log("[s: %zu%s%s]", d->size,
          d->flags & SX_POOL_DATA_FLAG_CACHE ? " c" : "",
          d->flags & SX_POOL_DATA_FLAG_FREE ? " F" : "");
      count++;
    }
    bt_log("\n");
  }
  bt_log("}\n");
}

static inline
void sx_pool_print_cache(sx_pool_t * pool)
{

  bt_log("{(cache)\n");
  for (unsigned int i = 0; i < SX_POOL_FCACHE_UBITS_MAX; i++) {
    if (pool->fcache[i].size)
      bt_log("  [s: %u] * %zu\n", ((i + 1) << 4), pool->fcache[i].size);
  }
  bt_log("}\n");

}

#endif /* TEST */

void * sx_pool_getmem(sx_pool_t * pool, size_t size)
{
  if (!pool || !size)
    return NULL;

#if defined (TEST) && defined (VERBOSETEST)
  sx_pool_print(pool);
#endif

  size = ALIGN16(size);

  struct data_header * d = NULL;

  if (!pool->tail) {
    pool->tail = sx_pool_chunk_allocate(pool, SX_POOL_CHUNK_ALLOC);
    if (!pool->tail)
      return NULL;
    pool->head = pool->tail;
  }

  if (size >= SX_POOL_FCACHE_MIN && size <= SX_POOL_FCACHE_MAX) {
    struct cache_array * array = &pool->fcache[(size >> 4) - 1];
    if (array->size) {
      d = array->data[0];
      memmove(array->data, &array->data[1], (array->size - 1) * sizeof(struct data_header *));
      array->size--;
    }
  }

  struct chunk_header * c;
  if (!d) {
    struct data_header * t;

    c = pool->tail;
    while (!d && c) {
      t = sx_chunk_first_data(c);
      while (!d && t) {
        if (!(t->flags & SX_POOL_DATA_FLAG_FREE) || (t->flags & SX_POOL_DATA_FLAG_CACHE)) {
          t = sx_chunk_next_data(c, t);
        } else if (t->size + sizeof(struct data_header) < size) {
          t = sx_chunk_next_data(c, t);
        } else {
          if (t->size == size)
            d = t;
          else if (sx_data_splitable(t, size))
            d = sx_chunk_data_split(c, t, size);
          else
            d = t;
        }
      }
      c = c->prev;
    }
  }

  if (!d) {
    c = sx_pool_chunk_allocate(pool, size + sizeof(struct chunk_header));
    if (c) {
      c->prev = pool->tail;
      pool->tail = c;
      return sx_pool_getmem(pool, size);
    }
  }

  if (d)
    d->flags = 0;

#if defined (TEST) && defined (VERBOSETEST)
  sx_pool_print(pool);
  bt_log("END\n");
#endif

  return sx_pool_data_to_pointer(d);
}

static inline
struct data_header * sx_chunk_owns_data(struct chunk_header * chunk, void * data)
{
  char               * start = (char *) chunk + sizeof(struct chunk_header);
  char               * end = start + chunk->size;
  char               * pos = (char *) data - sizeof(struct data_header);
  struct data_header * d;

  if ((char *) data < start || (char *) data + sizeof(struct data_header) > end)
    return NULL;

  for (d = sx_chunk_first_data(chunk); d; d = sx_chunk_next_data(chunk, d)) {
    if ((char *) d == pos)
      break;
  }

  return d;
}

static inline
int sx_pool_chunk_empty(struct chunk_header * chunk)
{
  struct data_header * d;

  d = sx_chunk_first_data(chunk);

  if (d->flags & SX_POOL_DATA_FLAG_FREE)
    if (d->size + sizeof(struct data_header) == chunk->size)
      return 1;

  return 0;
}

void sx_pool_normalize(sx_pool_t * pool)
{
  struct chunk_header * chunk;
  struct chunk_header * chunk_next;
  struct chunk_header * chunk_prev;
  struct data_header  * d, * p;

  /* drop chache */
  for (unsigned int i = 0; i < SX_POOL_FCACHE_UBITS_MAX; i++) {
    struct cache_array * array = &pool->fcache[i];
    for (unsigned int j = 0; j < array->size; j++) {
      array->data[j]->flags &= ~SX_POOL_DATA_FLAG_CACHE;
    }
    memset(&pool->fcache[i], 0, sizeof(struct cache_array));
  }

  chunk = pool->tail;
  chunk_next = NULL;
  chunk_prev = chunk ? chunk->prev : NULL;
  for (; chunk; chunk_next = chunk, chunk = chunk_prev, chunk_prev = chunk ? chunk->prev : NULL) {
    p = sx_chunk_first_data(chunk);
    d = sx_chunk_next_data(chunk, p);
    for (; p; p = d, d = d ? sx_chunk_next_data(chunk, d) : NULL) {
      if (p && d) {
        if (p->flags & SX_POOL_DATA_FLAG_FREE && d->flags & SX_POOL_DATA_FLAG_FREE) {
          if (!(p->flags & SX_POOL_DATA_FLAG_CACHE) && !(d->flags & SX_POOL_DATA_FLAG_CACHE)) {
            p->size += d->size + sizeof(struct data_header);
            d = p;
            continue;
          }
        }
      }

      if (p && p->flags & SX_POOL_DATA_FLAG_FREE) {
        if (!(p->flags & SX_POOL_DATA_FLAG_CACHE)) {

          if (p->size >= SX_POOL_FCACHE_MIN && p->size <= SX_POOL_FCACHE_MAX) {
            struct cache_array * array = &pool->fcache[(p->size >> 4) - 1];
            if (array->size < SX_POOL_FCACHE_SIZE) {
              array->data[array->size] = p;
              p->flags |= SX_POOL_DATA_FLAG_CACHE;
              array->size++;
              continue;
            }
          }
        }
      }
    }
    if (sx_pool_chunk_empty(chunk)) {
      if (chunk_next)
        chunk_next->prev = chunk_prev;
      if (chunk != pool->tail) {
        chunk->prev = pool->tail;
        pool->tail = chunk;
        chunk = chunk_next;
      }
    }
  }

  chunk = pool->tail;
  chunk_prev = chunk ? chunk->prev : NULL;
  for (; chunk; chunk = chunk_prev, chunk_prev = chunk ? chunk->prev : NULL) {
    if (chunk !=pool->head && sx_pool_chunk_empty(chunk)) {
      pool->tail = chunk_prev;
      free(chunk);
      continue;
    }
    break;
  }
}

void sx_pool_retmem(sx_pool_t * pool, void * data)
{
  struct chunk_header * c, * p;
  struct data_header  * d;


#if defined (TEST) && defined (VERBOSETEST)
  sx_pool_print(pool);
#endif

  for (c = pool->tail, p = NULL; c; c = c->prev) {
    if ((d = sx_chunk_owns_data(c, data))) {
      p = c;
      break;
    }
  }

  if (!p || !d)
    return;

  d->flags |= SX_POOL_DATA_FLAG_FREE;

  if (d->size >= SX_POOL_FCACHE_MIN && d->size <= SX_POOL_FCACHE_MAX) {
    struct cache_array * array = &pool->fcache[(d->size >> 4) - 1];
    if (array->size < SX_POOL_FCACHE_SIZE) {
      array->data[array->size] = d;
      d->flags |= SX_POOL_DATA_FLAG_CACHE;
      array->size++;
    }
  }

  if (pool->counter++ % 32 == 0)
    sx_pool_normalize(pool);

#if defined (TEST) && defined (VERBOSETEST)
  sx_pool_print(pool);
  bt_log("END\n");
#endif

}

#include "sx-pool-test.c"
