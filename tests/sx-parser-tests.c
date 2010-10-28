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

int sx_atom_test_escape_print(sx_t * sx, int deuni)
{
  char ss = 0, * p;
  struct sx_ucr u;

  memset(&u, 0, sizeof(u));

  if (sx->isatom) {
    if (sx->isplain)
      bt_log("%.*s%s",
          sx->atom.string->used, sx->atom.string->buffer, sx->atom.string->used > 30 ? "\n" : "");
    else if (sx->isuint) {
      bt_log("%lu", sx->atom.uint);
      if (sx->atom.uint > 100000000)
        return 1;
    } else if (sx->issint) {
      bt_log("%ld", sx->atom.sint);
      if (sx->atom.sint > 100000000 || sx->atom.sint < -100000000)
        return 1;
    } else if (sx->issquot || sx->isdquot) {
      if (sx->issquot)
        ss = '\'';
      else if (sx->isdquot)
        ss = '\"';

      bt_log("%c", ss);

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
            bt_log("\\0"); break;
          case '\a':
            bt_log("\\a"); break;
          case '\b':
            bt_log("\\b"); break;
          case '\f':
            bt_log("\\f"); break;
          case '\n':
            bt_log("\\n"); break;
          case '\r':
            bt_log("\\r"); break;
          case '\t':
            bt_log("\\t"); break;
          case '\v':
            bt_log("\\v"); break;
          case '\\':
            bt_log("\\\\"); break;
          default:
            if (p[i] == ss)
              bt_log("\\%c", u.c);
            else {
              if (u.c <= 127)
                bt_log("%c", u.c);
              else {
                if (deuni)
                  bt_log("\\u%04x", u.c);
                else
                  bt_log("%.*s", u.l, u.n);
              }
            }
        }
        u.l = 0;
      }

      bt_log("%c", ss);

      if (sx->atom.string->used > 28)
        return 1;
    }
  }
  return 0;
}

void sx_test_print(sx_t * sx, unsigned int level)
{
  int lila = 0;
#if 1
  while (sx) {
    if (lila) {
      bt_log(" ");
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
      if (sx_atom_test_escape_print(sx, 0))
        bt_log("\n%*.0s", level, "");
      else
        bt_log(" ");
    } else if (sx->islist) {
      if (!lila)
        bt_log("\n%*.0s(", level, "");
      else
        bt_log("(");
      sx_test_print(sx->list, level + 1);
      bt_log(")");
      lila = 1;
    }
    sx = sx->next;
  }
#endif
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
    if (err.composite) {
      bt_log("parser error: %s at line %u\n", sx_parser_strerror(parser), parser->line);
      chunk = sx_strgen_get(parser->gen);
      if (chunk) {
        bt_log("last chunk was: >>%.*s<<\n", chunk->used, chunk->buffer);
        sx_pool_retmem(parser->pool, chunk);
        goto out;
      }
    }
    bt_assert_int_equal(err.composite, 0);
  }
  sx_test_print(parser->root, 0);

out:
  sx_parser_clear(parser);
  bt_assert_int_equal(err.composite, 0);

  return BT_RESULT_OK;
}
#endif /* TEST */
