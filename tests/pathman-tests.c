#ifdef TEST
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/

#include <bt.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>

BT_SUITE_DEF(pathman, "pathman tests");

struct pathman_test {
  pathman_t pman[1];
  mem_allocator_t a[1];

  size_t num;
  char * flag;
  char ** strv;
};

BT_SUITE_SETUP_DEF(pathman, objectref)
{
  struct pathman_test * test = malloc(sizeof(struct pathman_test));
  char               buf[128];
  FILE             * fp;
  uint16_t           n;

  bt_assert_int_equal(sizeof(union paccess), sizeof(uint64_t));

  bt_assert_ptr_not_equal(test, NULL);

  n = 0;
  fp = fopen("tests/pathman-tests.txt", "r");
  if (!fp)
    return BT_RESULT_IGNORE;

  while (fgets(buf, 512, fp)) {
    n++;
  }

  test->num = n;

  test->strv = malloc(sizeof(char *) * test->num);
  bt_assert_ptr_not_equal(test->strv, NULL);
  memset(test->strv, 0, sizeof(char *) * test->num);

  test->flag = malloc(sizeof(char *) * test->num);
  bt_assert_ptr_not_equal(test->flag, NULL);
  memset(test->flag, 0, test->num);

  fseek(fp, 0, SEEK_SET);

  n = 0;
  while (n < test->num && fgets(buf, 512, fp)) {
    if (buf[0] == 'd')
      test->flag[n] = 1;
    else if (buf[0] == 'f')
      test->flag[n] = 2;

    test->strv[n++] = strndup(buf+2, strlen(buf+2) - 1);
  }

  fclose(fp);

  test->a->realloc = plain_realloc;
  test->a->ud = NULL;

  bt_assert_ptr_not_equal(pathman_init(test->pman, test->a), NULL);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(pathman, insert_and_find, object, "insert and find")
{
  struct pathman_test * test = object;
  struct plookup dlup;
  struct plookup flup;

  for (unsigned k = 0; k < test->num; k++) {
    if (test->flag[k] == 1) {
      // bt_log("DIR '%s'\n", test->strv[k]);
      dlup = pathman_add_dir(test->pman, test->strv[k], 0);
      if (dlup.err) {
        bt_log("FAIL dir '%s'\n", test->strv[k]);
        bt_chkerr(dlup.err);
      }
      bt_assert_ptr_not_equal(dlup.dir, NULL);
      bt_assert_int_not_equal(dlup.dir->state.top.index, 0);
    } else if (test->flag[k] == 2 && dlup.dir) {
      // bt_log("FILE '%s'\n", basename(test->strv[k]));
      flup = pathman_add_file(test->pman, dlup.dir, basename(test->strv[k]), 0);
      if (flup.err) {
        bt_log("FAIL file '%s'\n", basename(test->strv[k]));
        bt_chkerr(flup.err);
      }
    }
  }

  unsigned c = 0;
  tnode_bank_safe_foreach(bank, test->pman->trie->nodes)
    c++;

  bt_log("pathman: trie consists of %d bank(s)\n", c);

  //trie_print(test->pman->trie, 4);
  //pathman_print(test->pman, 4);

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(pathman, objectref)
{
  struct pathman_test * test = *objectref;

  pathman_clear(test->pman);

  free(test->strv);
  free(test->flag);

  free(test);

  return BT_RESULT_OK;
}


#endif /* TEST */
