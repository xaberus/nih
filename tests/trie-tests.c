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
# include <stdio.h>

BT_SUITE_DEF(trie, "trie tests");

BT_TEST_DEF_PLAIN(trie, insert_and_find, "insert and find")
{
  struct trie trie;
  trie_init(&trie);
  char buf[128];
  FILE * fp;
  uint16_t n, data;
  err_t err;

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  if (!fp)
    return BT_RESULT_IGNORE;

  while(fgets(buf, 512, fp)) {
    //bt_log("INS '%.*s' -> %d\n", (int)strlen(buf)-1,  buf, n);

    err = trie_insert(&trie, (const uint8_t *)buf, strlen(buf)-1, n++);
    if (err.composite)
      bt_log("FAIL:  '%.*s'\n", (int)strlen(buf)-1,  buf);

    bt_chkerr(err);
  }
  fclose(fp);

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  while(fgets(buf, 512, fp)) {
    //bt_log("FND '%.*s' -> %d\n", (int)strlen(buf)-1,  buf, tuple.node->data);
    err = trie_find(&trie, (const uint8_t *)buf, strlen(buf)-1, &data);
    bt_chkerr(err);
    bt_assert_int_equal(data, n);
    n++;
  }

  /* trie_print(&trie); */

  fclose(fp);

  trie_clear(&trie);

  return BT_RESULT_OK;
}

#endif /* TEST */
