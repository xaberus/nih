#ifndef _TABLE_H
#define _TABLE_H

#include "err.h"
#include <stdint.h>

#include "trie.h"

enum table_error {
  TABLE_ERROR_SUCCESS = 0,
  TABLE_ERROR_DUPLICATE,
  TABLE_ERROR_CORRUPTION,
  TABLE_ERROR_NOT_FOUND,
};

#define TVALUE_BASE() \
  struct {\
    uint16_t tag; \
  }

enum tvalue_type {
  TVALUE_NONE = 0,
  TVALUE_TABLE,
  TVALUE_INTEGER,
  TVALUE_UINTEGER,
  TVALUE_REFERENCE,
};

struct table {
  trie_t      trie[1];

  uintptr_t * array;
  uint32_t    array_size;
  uint32_t    array_alloc;

  mem_allocator_t a;
};
typedef struct table table_t;

__extension__ struct tvalue_base {
  TVALUE_BASE();
};


__extension__ struct tvalue_table {
  TVALUE_BASE();
  table_t table;
};

__extension__ struct tvalue_integer {
  TVALUE_BASE();
  int64_t integer;
};

__extension__ struct tvalue_uinteger {
  TVALUE_BASE();
  uint64_t uinteger;
};

typedef int ref_t;

__extension__ struct tvalue_reference {
  TVALUE_BASE();
  ref_t reference;
};

table_t * table_init(table_t * table, mem_allocator_t a);
void      table_clear(table_t * table);

err_t     table_set(table_t * table, const char key[], uint16_t len, struct tvalue_base * value);
err_t     table_seti(table_t * table, uint32_t key, struct tvalue_base * value);

typedef struct { err_t error; struct tvalue_base * value; } tget_t;

tget_t    table_get(table_t * table, const char key[], uint16_t len);
tget_t    table_geti(table_t * table, uint32_t key);

#endif /* _TABLE_H */
