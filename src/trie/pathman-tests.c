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

struct pathman_test {
  pathman_t   pman[1];
  size_t      num;
  char      * flag;
  char     ** strv;
};

int pathman_test_setup(void * object, void ** objectref)
{
  UNUSED_PARAM(object);
  struct pathman_test * test = malloc(sizeof(struct pathman_test));
  char                  buf[128];
  FILE                * fp;
  uint16_t              n;

  bt_assert_int_equal(sizeof(union paccess), sizeof(uint64_t));

  bt_assert_ptr_not_equal(test, NULL);

  n = 0;
  if (!(fp = fopen(BROOT "/src/trie/pathman-tests.txt", "r"))) {
    printf("could not open test file for reading!\n");
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

  bt_chkerr(pathman_init(test->pman));

  printf("[pathman:init] file bank size is %u\n", PDIR_BANKSIZE);
  printf("[pathman:init] dir bank size is %u\n", PFILE_BANKSIZE);

  *objectref = test;

  return BT_RESULT_OK;
}

int pathman_test_teardown(void * object, void ** objectref)
{
  struct pathman_test * test = object;

  pathman_clear(test->pman);

  for (unsigned k = 0; k < test->num; k++) {
    free(test->strv[k]);
  }
  free(test->strv);
  free(test->flag);

  free(test);

  bt_chkerr(err_pop());
  *objectref = NULL;
  return BT_RESULT_OK;
}

BT_TEST_FIXTURE(pathman, border, pathman_test_setup, pathman_test_teardown, object)
{
  struct pathman_test * test = object;
  e_pdir_t dlup;
  e_pfile_t flup;

  dlup = pathman_add_dir(test->pman, "/a", 0);
  bt_chkerr(dlup.err);
  bt_assert_ptr_not_equal(dlup.dir, NULL);
  flup = pathman_add_file(test->pman, dlup.dir, "foo/bar", 0);
  bt_assert(flup.err);
  err_reset();

  return BT_RESULT_OK;
}

BT_TEST_FIXTURE(pathman, insert_and_find, pathman_test_setup, pathman_test_teardown, object)
{
  struct pathman_test * test = object;
  e_pdir_t dlup = {NULL, NULL};
  e_pfile_t flup;

  uint32_t fnum = 0, dnum = 0;

  for (uint32_t k = 0; k < test->num; k++) {
    if (test->flag[k] == 1) {
      // printf("DIR '%s'\n", test->strv[k]);
      dlup = pathman_add_dir(test->pman, test->strv[k], 0);
      if (dlup.err) {
        printf("FAIL dir '%s'\n", test->strv[k]);
        bt_chkerr(dlup.err);
      }
      bt_assert_ptr_not_equal(dlup.dir, NULL);
      bt_assert_int_not_equal(dlup.dir->state.top.index, 0);
      dlup.dir->rid = k;
      dnum++;
    } else if (test->flag[k] == 2 && dlup.dir) {
      // printf("FILE '%s'\n", basename(test->strv[k]));
      flup = pathman_add_file(test->pman, dlup.dir, basename(test->strv[k]), 0);
      if (flup.err) {
        printf("FAIL file '%s'\n", basename(test->strv[k]));
        bt_chkerr(flup.err);
      }
      flup.file->rid = k;
      fnum++;
    }
  }

  char buf[1024], * s;
  uint16_t dl = 0, fl = 0;

  for (unsigned k = 0; k < test->num; k++) {
    if (test->flag[k] == 1) {
      s = test->strv[k]; dl = strlen(s);
      memcpy(buf, s, dl);
      if (buf[dl - 1] != '/') {
        buf[dl++] = '/';
      }
      buf[dl] = '\0';

      // printf("D %s\n", buf);

      dlup = pathman_get_dir(test->pman, buf);
      if (dlup.err) {
        printf("FAIL dir '%s'\n", test->strv[k]);
        bt_chkerr(dlup.err);
      }
      bt_assert_ptr_not_equal(dlup.dir, NULL);
    } else if (test->flag[k] == 2 && dlup.dir) {
      bt_assert(dl + fl < 1023);
      s = test->strv[k]; fl = strlen(s);
      memcpy(buf + dl, s, fl);
      buf[dl + fl++] = '\0';

      // printf("F %s\n", buf);

      flup = pathman_get_file(test->pman, buf);
      if (flup.err) {
        printf("FAIL file '%s'\n", basename(test->strv[k]));
        bt_chkerr(flup.err);
      }
      bt_assert_ptr_not_equal(flup.file, NULL);
      //bt_assert(flup.file->rid == k);
    }
  }

  printf("[pathman] %u file(s) in %u dir(s)\n", fnum, dnum);
  printf("[pathman] %u trie bank(s)\n", test->pman->trie->banks);
  printf("[pathman] %u dir bank(s)\n", test->pman->dbanks);
  printf("[pathman] %u file bank(s)\n", test->pman->fbanks);

  // trie_print(test->pman->trie, 4);
  // pathman_print(test->pman, 4);

  return BT_RESULT_OK;
}

#endif /* TEST */
