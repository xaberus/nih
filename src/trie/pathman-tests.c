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
  pathman_t   pman[1];
  mema_t a[1];
  size_t      num;
  char      * flag;
  char     ** strv;
};

BT_SUITE_SETUP_DEF(pathman, objectref)
{
  struct pathman_test * test = malloc(sizeof(struct pathman_test));
  char                  buf[128];
  FILE                * fp;
  uint16_t              n;

  bt_assert_int_equal(sizeof(union paccess), sizeof(uint64_t));

  bt_assert_ptr_not_equal(test, NULL);

  test->a->realloc = plain_realloc;
  test->a->ud = NULL;

  n = 0;
  if (!(fp = fopen(BROOT "/src/trie/pathman-tests.txt", "r"))) {
    bt_log("could not open test file for reading!\n");
    return BT_RESULT_IGNORE;
  }

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

    test->strv[n++] = strndup(buf + 2, strlen(buf + 2) - 1);
  }

  fclose(fp);

  bt_assert_ptr_not_equal(pathman_init(test->a, test->pman), NULL);

  bt_log("[pathman:init] file bank size is %u\n", PDIR_BANKSIZE);
  bt_log("[pathman:init] dir bank size is %u\n", PFILE_BANKSIZE);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(pathman, border, object, "border cases")
{
  struct pathman_test * test = object;
  struct plookup        dlup, flup;

  dlup = pathman_add_dir(test->pman, "/a", 0);
  bt_chkerr(dlup.err);
  bt_assert_ptr_not_equal(dlup.dir, NULL);
  flup = pathman_add_file(test->pman, dlup.dir, "foo/bar", 0);
  bt_assert(flup.err == ERR_IN_INVALID);

  return BT_RESULT_OK;
}

BT_TEST_DEF(pathman, insert_and_find, object, "insert and find")
{
  struct pathman_test * test = object;
  struct plookup        dlup = {ERR_SUCCESS, pstate(tnode_tuple(NULL, 0)), NULL, NULL};
  struct plookup        flup;

  uint32_t fnum = 0, dnum = 0;

  for (uint32_t k = 0; k < test->num; k++) {
    if (test->flag[k] == 1) {
      // bt_log("DIR '%s'\n", test->strv[k]);
      dlup = pathman_add_dir(test->pman, test->strv[k], 0);
      if (dlup.err) {
        bt_log("FAIL dir '%s'\n", test->strv[k]);
        bt_chkerr(dlup.err);
      }
      bt_assert_ptr_not_equal(dlup.dir, NULL);
      bt_assert_int_not_equal(dlup.dir->state.top.index, 0);
      dlup.dir->rid = k;
      dnum++;
    } else if (test->flag[k] == 2 && dlup.dir) {
      // bt_log("FILE '%s'\n", basename(test->strv[k]));
      flup = pathman_add_file(test->pman, dlup.dir, basename(test->strv[k]), 0);
      if (flup.err) {
        bt_log("FAIL file '%s'\n", basename(test->strv[k]));
        bt_chkerr(flup.err);
      }
      flup.file->rid = k;
      fnum++;
    }
  }

  char buf[1024], * s;
  uint16_t dl, fl;

  for (unsigned k = 0; k < test->num; k++) {
    if (test->flag[k] == 1) {
      s = test->strv[k]; dl = strlen(s);
      memcpy(buf, s, dl);
      if (buf[dl - 1] != '/') {
        buf[dl++] = '/';
      }
      buf[dl] = '\0';

      // bt_log("D %s\n", buf);

      dlup = pathman_lookup(test->pman, buf);
      if (dlup.err) {
        bt_log("FAIL dir '%s'\n", test->strv[k]);
        bt_chkerr(dlup.err);
      }
      bt_assert(dlup.state.node.isdir);
      bt_assert_ptr_not_equal(dlup.dir, NULL);
    } else if (test->flag[k] == 2 && dlup.dir) {
      bt_assert(dl + fl < 1023);
      s = test->strv[k]; fl = strlen(s);
      memcpy(buf + dl, s, fl);
      buf[dl + fl++] = '\0';

      // bt_log("F %s\n", buf);

      flup = pathman_lookup(test->pman, buf);
      if (flup.err) {
        bt_log("FAIL file '%s'\n", basename(test->strv[k]));
        bt_chkerr(flup.err);
      }
      bt_assert(flup.state.node.isfile);
      bt_assert_ptr_not_equal(flup.file, NULL);
      //bt_assert(flup.file->rid == k);
    }
  }

  bt_log("[pathman] %u file(s) in %u dir(s)\n", fnum, dnum);
  bt_log("[pathman] %u trie bank(s)\n", test->pman->trie->banks);
  bt_log("[pathman] %u dir bank(s)\n", test->pman->dbanks);
  bt_log("[pathman] %u file bank(s)\n", test->pman->fbanks);

  // trie_print(test->pman->trie, 4);
  // pathman_print(test->pman, 4);

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(pathman, objectref)
{
  struct pathman_test * test = *objectref;

  pathman_clear(test->pman);

  for (unsigned k = 0; k < test->num; k++) {
    free(test->strv[k]);
  }
  free(test->strv);
  free(test->flag);

  free(test);

  return BT_RESULT_OK;
}


#endif /* TEST */
