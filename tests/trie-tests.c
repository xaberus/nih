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

BT_SUITE_DEF(trie, "trie tests");

struct trie_test {
  struct trie trie[1];
  mem_allocator_t a[1];

  size_t  num;
  char ** strv;
  char  * flag;

};

BT_SUITE_SETUP_DEF(trie, objectref)
{
  struct trie_test * test = malloc(sizeof(struct trie_test));

  char        buf[128];
  FILE      * fp;
  uint16_t    n;

  bt_assert_ptr_not_equal(test, NULL);

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
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
    test->strv[n++] = strndup(buf, strlen(buf) - 1);
  }

  fclose(fp);

  test->a->realloc = plain_realloc;
  test->a->ud = NULL;

  bt_assert_ptr_not_equal(trie_init(test->trie, test->a), NULL);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, insert_and_find, object, "insert and find")
{
  struct trie_test * test = object;

  uint16_t    n;
  uint64_t    data;
  err_t       err;

  trie_t * trie = test->trie;
  size_t num = test->num;
  char ** strv = test->strv;

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0);
      if (err)
        bt_log("FAIL:  '%.*s'\n", (int) strlen(str), str);
      bt_chkerr(err);
    }
  }

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  srand(1);

  for (size_t i = 0; i < num; i++) {
    char * str = NULL;

    while (!str) {
      n = rand() % num;
      str = strv[n];
    }

    err = trie_delete(trie, strlen(str), (const uint8_t *) str);
    bt_chkerr(err);

    free(str);
    strv[n] = NULL;

    for (size_t j = 0; j < num; j++) {
      str = strv[j];
      if (str) {
        err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
        bt_chkerr(err);
        bt_assert_int_equal(data, j);
      }
    }
  }

  return BT_RESULT_OK;
}


int test_trie_ff(uint16_t len, const uint8_t word[len], uintptr_t data, void * ud)
{
  (void) word; (void) len; (void) data; (void) ud;

  // printf("'%.*s' -> %zu\n", (int) len, (const char *) word, data); fflush(stdout);

  return 0;
}

BT_TEST_DEF(trie, insert_delete_insert, object,"insert delete insert")
{
  struct trie_test * test = object;

  uint64_t    data;
  err_t       err;

  trie_t * trie = test->trie;
  size_t num = test->num;
  char ** strv = test->strv;
  char * flag = test->flag;

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0);
      bt_chkerr(err);
    }
  }

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  trie_foreach(trie, test_trie_ff, NULL);

  srand(1);

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];

    if (!flag[j] && str) {
      err = trie_delete(trie, strlen(str), (const uint8_t *) str);
      bt_chkerr(err);
      flag[j] = 1;
    }
  }

  srand(1);

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];
    if (flag[j] && str) {
      err = trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0);
      bt_chkerr(err);
      flag[j] = 0;
    }
  }

  /* no allocations happened */

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  // trie_print(trie, 4);

  for (size_t j = 0; j < num; j++) {
    free(strv[j]);
  }

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(trie, objectref)
{
  struct trie_test * test = *objectref;

  trie_clear(test->trie);

  free(test->strv);
  free(test->flag);

  free(test);

  return BT_RESULT_OK;
}


#endif /* TEST */
