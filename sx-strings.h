#ifndef _SX_STRINGS_H
#define _SX_STRINGS_H

#include <stdlib.h>
#include <stdint.h>

struct sx_str {
  uint32_t length;
  uint32_t used;
  char buffer[];
};

#define SX_STR_ALLOC_SIZE(length) (sizeof(struct sx_str) + (length))
#define SX_STR_SIZE_FROM_ALLOC(_alloc) ((_alloc) - sizeof(struct sx_str))

typedef struct sx_str sx_str_t;

#include "sx-str-stack.h"


struct sx_strgen {
  sx_str_stack_t stack[1];
};

#define SX_STRGEN_START_ALLOC 128

typedef struct sx_strgen sx_strgen_t;

struct sx_strgen * sx_strgen_init(sx_strgen_t * gen);
struct sx_strgen * sx_strgen_reset(sx_strgen_t * gen);
void               sx_strgen_clear(sx_strgen_t * gen);

size_t             sx_strgen_append(sx_strgen_t * gen, const char * str, size_t length);
size_t             sx_strgen_append_char(sx_strgen_t * gen, const char c);
sx_str_t *         sx_strgen_get(sx_strgen_t * gen);

#endif /* _SX_STRINGS_H */
