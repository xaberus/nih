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
# include <string.h>

#define PRINT(...) dprintf(STDOUT_FILENO, __VA_ARGS__)

int sx_atom_test_escape_print(sx_t * sx, int deuni)
{
  char ss = 0, * p;
  struct sx_ucr u;
  int len = 0;

  memset(&u, 0, sizeof(u));

  if (sx->isatom) {
    if (sx->isplain)
      return PRINT("%.*s", sx->atom.string->used, sx->atom.string->buffer);
    else if (sx->isuint)
      return PRINT("%lu", sx->atom.uint);
    else if (sx->issint)
      return PRINT("%ld", sx->atom.sint);
    else if (sx->issquot || sx->isdquot) {
      if (sx->issquot)
        ss = '\'';
      else if (sx->isdquot)
        ss = '\"';

      len += PRINT("%c", ss);

      p = sx->atom.string->buffer;
      for (unsigned int i = 0; i < sx->atom.string->used; i++) {
        if (sx_parser_decode_utf8(&u.s, &u.c, p[i]) != 0) {
          if (u.s != 1) {
            u.n[u.l++] = p[i];
            continue;
          }
          return 0;
        }
        u.n[u.l++] = p[i];

        switch (u.c) {
          case '\0':
            len += PRINT("\\0"); break;
          case '\a':
            len += PRINT("\\a"); break;
          case '\b':
            len += PRINT("\\b"); break;
          case '\f':
            len += PRINT("\\f"); break;
          case '\n':
            len += PRINT("\\n"); break;
          case '\r':
            len += PRINT("\\r"); break;
          case '\t':
            len += PRINT("\\t"); break;
          case '\v':
            len += PRINT("\\v"); break;
          case '\\':
            len += PRINT("\\\\"); break;
          default:
            if (p[i] == ss)
              len += PRINT("\\%c", u.c);
            else {
              if (u.c <= 127)
                len += PRINT("%c", u.c);
              else {
                if (deuni)
                  len += PRINT("\\u%04x", u.c);
                else {
                  PRINT("%.*s", u.l, u.n);
                  len ++;
                }
              }
            }
        }
        u.l = 0;
      }

      len += PRINT("%c", ss);

      return len;
    }
  }
  return 0;
}

static inline
int sx_is_lspa(sx_t * sx)
{
  int spa = 0;
  if (!sx || !sx->next)
      return 0;

  sx = sx->next;

  while (sx) {
    if (!sx->islist)
      return 0;
    sx = sx->next;
    spa ++;
  }
  return spa > 1;
}

int sx_test_print(sx_t * sx, unsigned int level, int len, unsigned int brk, unsigned int sl)
{
  int lila = 0;
#if 1
  while (sx) {
    if (lila) {
      len += PRINT(" ");
      lila = 0;
    }

    if (sx->isatom) {
        /*bt_log("atom(%s%s%s%s%s%s)",
          (sx->isplain? "p" : ""),
          (sx->issquot? "s" : ""),
          (sx->isdquot? "d" : ""),
          (sx->issint? "I" : ""),
          (sx->isuint? "U" : ""),
          (sx->isdouble? "D" : ""));*/
      len += sx_atom_test_escape_print(sx, 0);
    } else if (sx->islist) {
      if (sl != level && (len > 40 || (sx->list && sx->list->islist) || (sx_is_lspa(sx->list)))) {
        len += PRINT("\n%*.0s(", level, "");
        len = 0;
        brk = level;
      } else
        len += PRINT("(");
      len += sx_test_print(sx->list, level + 1, len, brk,  sl + 1);
      len += PRINT(")");
      lila = 1;
    }


    if (sx->next && !lila) {
      if (
        (len >= 70 || (sx->isatom && (sx->issquot || sx->isdquot)
        && sx->atom.string->used > 30))) {
      len = 0;
      PRINT("\n%*.0s", level, "");
    } else if (len > 0)
      len += PRINT(" ");
    }

    if (sl > 0)
      sx = sx->next;
    else
      break;
  }
#endif
  return len;
}

BT_TEST_DEF_PLAIN(sx_util, sx_parser, "pokes sx parser and watches out for errors (prequisite only)")
{
  sx_parser_t     parser[1];
  char            t[512];
  struct stat     sb;
  ssize_t         rd;
  int             fd;
  err_t           err;
  sx_str_t      * chunk;

  const char * file = "tests/sx-parser-tests.txt";

  if (stat(file, &sb)) {
    bt_log("could not find %s: %s\n", file, strerror(errno));
    return BT_RESULT_IGNORE;
  }

  fd = open(file, O_RDONLY);
  if (fd == -1) {
    bt_log("could not open %s: %s\n", file, strerror(errno));
    return BT_RESULT_FAIL;
  }

  bt_assert_ptr_not_equal(sx_parser_init(parser), NULL);

  while ((rd = read(fd, t, 2))) {
    /*bt_log(">> %.*s\n", (int)rd, t);*/
    err = sx_parser_read(parser, t, rd);
    if (err) {
      bt_log("parser error: %s at line %u\n", sx_parser_strerror(parser), parser->line);
      chunk = sx_strgen_get(parser->gen);
      if (chunk) {
        bt_log("last chunk was: >>%.*s<<\n", chunk->used, chunk->buffer);
        sx_pool_retmem(parser->pool, chunk);
        goto out;
      }
    }
    bt_assert_int_equal(err, 0);
  }
  sx_test_print(parser->root, 0, 0, 0, 1);

out:
  sx_parser_clear(parser);
  bt_assert_int_equal(err, 0);

  return BT_RESULT_OK;
}
#endif /* TEST */
