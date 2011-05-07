#include <string.h>
#include "sx-strings.h"

struct sx_strgen * sx_strgen_init(sx_strgen_t * gen)
{
  sx_str_t * str = calloc(1, SX_STR_ALLOC_SIZE(SX_STRGEN_START_ALLOC));
  if (!str) {
    sx_str_stack_clear(gen->stack);
    return NULL;
  }

  str->length = SX_STRGEN_START_ALLOC;
  str->used = 0;

  if (!sx_str_stack_push(gen->stack, str)) {
    sx_str_stack_clear(gen->stack);
    free(str);
    return NULL;
  }

  return gen;
}

#define ALIGN128(_size) (((_size) + 127L) & ~127L)
#define MAX(a, b) ((a)>(b) ? a : b)

size_t sx_strgen_append(sx_strgen_t * gen, const char * str, size_t length)
{
  sx_str_t * sxstr = sx_str_stack_top(gen->stack);

  if (!str)
    return 0;

append:
  if (length + sxstr->used + 1 < sxstr->length) {
    memcpy(sxstr->buffer + sxstr->used, str, length);
    sxstr->used += length;
    return length;
  }

  size_t alloc = ALIGN128(SX_STR_ALLOC_SIZE(MAX(SX_STRGEN_START_ALLOC, length + 1)));
  sxstr = calloc(1, alloc);
  if (!sxstr)
    return 0;
  if (!sx_str_stack_push(gen->stack, sxstr)) {
    free(sxstr);
    return 0;
  }
  sxstr->length = SX_STR_SIZE_FROM_ALLOC(alloc);
  sxstr->used = 0;

  goto append;
}

size_t sx_strgen_append_char(sx_strgen_t * gen, const char c)
{
  return sx_strgen_append(gen, &c, 1);
}

sx_str_t * sx_strgen_get(sx_strgen_t * gen)
{
  struct sx_str_stack_page * page = NULL;
  size_t                     length;
  size_t                     pos;
  sx_str_t                 * sxstr;

  length = 0;
  for (page = gen->stack->tail; page; page = page->prev) {
    for (unsigned int i = 0; i < page->count; i++) {
      length += page->data[i]->used;
    }
  }

  if (!length)
    return NULL;

  sxstr = calloc(1, SX_STR_ALLOC_SIZE(length + 1));
  if (!sxstr)
    return NULL;

  sxstr->length = length + 1;
  pos = length;
  sxstr->buffer[pos] = '\0';

  for (page = gen->stack->tail; page; page = page->prev) {
    if (page->count) {
      for (unsigned int i = page->count - 1; i + 1 > 0; i--) {
        pos -= page->data[i]->used;
        memcpy(&sxstr->buffer[pos], page->data[i]->buffer, page->data[i]->used);
      }
    }
  }

  sxstr->used = length;

  return sxstr;
}

void sx_strgen_clear(sx_strgen_t * gen)
{
  sx_str_t * sxstr;

  while ((sxstr = sx_str_stack_top(gen->stack))) {
    free(sxstr);
    sx_str_stack_pop(gen->stack);
  }
  sx_str_stack_clear(gen->stack);
}

struct sx_strgen * sx_strgen_reset(sx_strgen_t * gen)
{
  sx_strgen_clear(gen);
  if (!sx_strgen_init(gen))
    return NULL;
  return gen;
}

#ifdef TEST
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/

# include <bt.h>

# include <ctype.h>

static inline
void bt_dump_str(const char * str, size_t length)
{
  size_t       pos = 0;
  char         buff[3 * 16 + 2 + 16 + 2];
  char         tmp[16];
  unsigned int line;

  while (pos < length) {
    if (pos + 16 < length)
      line = 16;
    else
      line = length - pos;

    memcpy(tmp, str + pos, line);

    for (unsigned int i = 0; i < 16; i++) {
      if (i < line)
        snprintf(buff + 3 * i, 4, "%02hhx ", tmp[i]);
      else
        snprintf(buff + 3 * i, 4, "   ");
    }
    buff[3 * 16] = ' ';
    buff[3 * 16 + 1] = ' ';
    for (unsigned int i = 0; i < 16; i++) {
      if (i < line) {
        if (!isprint(tmp[i]))
          tmp[i] = '.';
      } else {
        tmp[i] = ' ';
      }
    }
    memcpy(buff + 3 * 16 + 2, tmp, 16);
    buff[3 * 16 + 2 + 16] = '\0';

    bt_log("%s\n", buff);

    pos += 16;
  }
}

struct strgen_test {
  sx_strgen_t gen[1];
};

BT_TEST_DEF_PLAIN(sx_util, sx_strgen, "strgen")
{
  struct strgen_test test[1];
  sx_str_t         * str = NULL;
  char               buff[10], * p;

  bt_assert_ptr_not_equal(sx_strgen_init(test->gen), NULL);

  for (unsigned int n = 0; n < 3; n++) {
    for (unsigned int i = 0; i < 1000; i++) {
      snprintf(buff, 10, "%08u", i);
      bt_assert_int_equal(sx_strgen_append(test->gen, buff, 8), 8);
    }

    bt_assert_ptr_not_equal((str = sx_strgen_get(test->gen)), NULL);
    bt_assert_int_equal(str->used, 8 * 1000);
    bt_assert(str->length >= 8 * 1000 + 1);


    p =  str->buffer;
    for (unsigned int i = 0; i < 1000; i++, p += 8) {
      snprintf(buff, 10, "%08u", i);
      if (strncmp(p, buff, 8) != 0) {
        bt_log("string cat failed: wanted %8s, got %8s\n", buff, p);
        bt_dump_str(str->buffer, 8 * 1000);
        return BT_RESULT_FAIL;
      }
    }
    free(str);
    sx_strgen_reset(test->gen);
  }


  bt_assert_ptr_equal((str = sx_strgen_get(test->gen)), NULL);

  sx_strgen_clear(test->gen);

  return BT_RESULT_OK;
}

#endif /* TEST */
