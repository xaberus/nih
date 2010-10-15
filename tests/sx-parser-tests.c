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

void sx_test_print(sx_t * sx, unsigned int level)
{
  while (sx) {
    bt_log("%*.0s[", level, "");
    if (sx->isatom) {
        bt_log("atom(%s%s%s%s%s%s)",
          (sx->isplain? "p" : ""),
          (sx->issquot? "s" : ""),
          (sx->isdquot? "d" : ""),
          (sx->issint? "I" : ""),
          (sx->isuint? "U" : ""),
          (sx->isdouble? "D" : ""));
      if (sx->isplain || sx->issquot || sx->isdquot)
        bt_log("<%.*s>(%u)",
          sx->atom.string->used, sx->atom.string->buffer, sx->line);
      else if (sx->isuint)
        bt_log("<%lu>(%u)", sx->atom.uint, sx->line);
      else if (sx->issint)
        bt_log("<%ld>(%u)", sx->atom.sint, sx->line);
    } else if (sx->islist) {
      bt_log("list] {\n");
      sx_test_print(sx->list, level + 1);
    }
    if (sx->islist)
      bt_log("%*.0s}\n", level, "");
    else
      bt_log("]\n");
    sx = sx->next;
  }
}

void sx_parser_free_nodes(sx_parser_t * parser, sx_t * sx)
{
  sx_t * f;

  while (sx) {
    f = sx; sx = sx->next;

    if (f->isatom)
      sx_atom_clear(f, parser);
    else if (f->islist)
      sx_parser_free_nodes(parser, f->list);

    sx_pool_retmem(parser->pool, f);
  }
}


BT_TEST_DEF_PLAIN(sx_util, parser, "strgen")
{
  sx_parser_t parser[1];
  char             t[128];
  struct stat      sb;
  ssize_t          rd;
  int              fd;
  err_t            err;

  memset(parser, 0, sizeof(parser));

  parser->s = SX_PARSER_INTERMEDIATE;
  parser->line = 1;


  if (stat("sx-parser-tests.txt", &sb)) {
    bt_log("could not find sx-parser-tests.txt: %s\n", strerror(errno));
    return BT_RESULT_IGNORE;
  }

  fd = open("sx-parser-tests.txt", O_RDONLY);
  if (fd == -1) {
    bt_log("could not open sx-parser-tests.txt: %s\n", strerror(errno));
    return BT_RESULT_FAIL;
  }


  sx_pool_init(parser->pool);
  bt_assert_ptr_not_equal(sx_stack_init(parser->pool, parser->stack), NULL);
  bt_assert_ptr_not_equal(sx_strgen_init(parser->pool, parser->gen), NULL);

  while ((rd = read(fd, t, 1))) {
    /*bt_log(">> %.*s\n", (int)rd, t);*/
    err = sx_parser_read(parser, t, rd);
    if (err.composite)
      bt_log("parser error %s at line %u\n", sx_parser_strerror(parser), parser->line);
    bt_assert_int_equal(err.composite, 0);
  }
  sx_test_print(parser->root, 0);

  sx_parser_free_nodes(parser, parser->root);
  sx_strgen_clear(parser->gen);
  sx_stack_clear(parser->stack);
  sx_pool_clear(parser->pool);

  return BT_RESULT_OK;
}
#endif /* TEST */
