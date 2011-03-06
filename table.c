#include "table.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>

table_t * table_init(table_t * table, mem_allocator_t a)
{
  if (!table)
    return NULL;

  table->a = a;

  if (!trie_init(table->trie, &table->a))
    return NULL;

  table->array = NULL;

  return table;
}

void      table_clear(table_t * table)
{
  trie_clear(table->trie);

  if (table->array)
    mem_free(&table->a, table->array);
}


err_t table_set(table_t * table, const char key[], uint16_t len, struct tvalue_base * value)
{
  if (!table || !key)
    return ERR_IN_NULL_POINTER;

  err_t err = trie_insert(table->trie, (const uint8_t *) key, len, (uintptr_t) value, 1);

  return err;
}

#define ALIGN64(_size) (((_size) + 63L) & ~63L)

err_t table_seti(table_t * table, uint32_t key, struct tvalue_base * value)
{
  if (!table || !key)
    return ERR_IN_NULL_POINTER;

  if(!table->array) {
    uintptr_t * tmp = mem_alloc(&table->a, sizeof(uintptr_t) * 64);
    if (!tmp)
      return ERR_MEM_ALLOC;

    memset(tmp, 0, sizeof(uintptr_t) * (64));

    table->array = tmp;
    table->array_size = 0;
    table->array_alloc = 64;
  }

  if (key >= table->array_alloc) {
    uintptr_t alloc = ALIGN64(key);

    if (alloc > UINT32_MAX)
      return ERR_IN_OVERFLOW;

    uintptr_t * tmp = 
      table->a.realloc(table->a.ud,
      table->array,
      sizeof(uintptr_t) * table->array_alloc,
      sizeof(uintptr_t) * alloc);

    if (!tmp)
      return ERR_MEM_REALLOC;

    memset(tmp + table->array_size, 0, sizeof(uintptr_t) * (table->array_alloc - table->array_size));

    table->array = tmp;
    table->array_alloc = alloc;
  }

  table->array[key] = (uintptr_t) value;

  if (key >= table->array_size)
    table->array_size = key + 1;

  return 0;
}

tget_t table_get(table_t * table, const char key[], uint16_t len)
{
  tget_t ret = {0, NULL};

  if (!table || !key) {
    ret.error = ERR_IN_NULL_POINTER;
    return ret;
  }

  ret.error = trie_find(table->trie, (const uint8_t *) key, len, (uintptr_t *) &ret.value);

  return ret;
}

tget_t table_geti(table_t * table, uint32_t key)
{
  tget_t ret = {0, NULL};

  if (!table || !key) {
    ret.error = ERR_IN_NULL_POINTER;
    return ret;
  }

  if (key > table->array_size) {
    ret.error = ERR_NOT_FOUND;
    return ret;
  }

  ret.value = (void *) table->array[key];
  return ret;
}

#include "tests/table-tests.c"

