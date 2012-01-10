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
#include <string.h>

//#include <google/profiler.h>

BT_SUITE_DEF(trie, "trie tests");

struct trie_test {
  trie_t      trie[1];
  size_t      num;
  char      * flag;
  char     ** strv;
};

BT_SUITE_SETUP_DEF(trie, objectref)
{
  struct trie_test * test = malloc(sizeof(struct trie_test));
  char               buf[512];
  FILE             * fp;
  uint16_t           n;

  bt_assert_ptr_not_equal(test, NULL);

  n = 0;
  if (!(fp = fopen(BROOT "/src/trie/trie-tests.txt", "r"))) {
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
    test->strv[n++] = strndup(buf, strlen(buf) - 1);
  }

  fclose(fp);

  err_r * e;
  e = trie_init(test->trie, 18);
  bt_assert_ptr_not_equal(e, NULL); bt_assert_int_equal(e->err, ERR_IN_INVALID);
  err_reset();
  e = trie_init(test->trie, 17);
  bt_assert_ptr_not_equal(e, NULL); bt_assert_int_equal(e->err, ERR_IN_INVALID);
  err_reset();
  e = trie_init(test->trie, 0);
  bt_assert_ptr_not_equal(e, NULL); bt_assert_int_equal(e->err, ERR_IN_INVALID);
  err_reset();

  bt_assert_ptr_equal(trie_init(test->trie, 8), NULL);

  bt_log("[trie:init] bank size is %u (masks are 0x%x 0x%x)\n",
    test->trie->banksize, test->trie->addrmask, test->trie->nodemask);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, str, object, "str mode")
{
  struct trie_test * test = object;
  trie_t           * trie = test->trie;
  struct trie_eppoit eppoit;
  ttuple_t tuple;

#define test(_str, _res, _val) \
  do { \
    eppoit = _trie_insert_decide(trie, tuple, strlen(_str), (const uint8_t *) (_str), 1); \
    bt_chkerr(eppoit.err); \
    if (eppoit.act == TRIE_INSERT_SPLIT_0_SET) \
      bt_log("'%s' -> ACT: %u, ~ {%.*s}\n", _str, eppoit.act, \
          eppoit.tuple.node->strlen, eppoit.tuple.node->str); \
    else if (eppoit.act == TRIE_INSERT_SPLIT_N_CHILD) \
      bt_log("'%s' -> ACT: %u, N: %d ~ [%c] {%.*s}\n", _str, eppoit.act, \
          eppoit.n, *(eppoit.tuple.node->str + eppoit.n), \
          eppoit.tuple.node->strlen - eppoit.n - 1, eppoit.tuple.node->str + eppoit.n + 1); \
    else if (eppoit.act == TRIE_INSERT_SPLIT_N_NEXT) \
      bt_log("'%s' -> ACT: %u, N: %d ~ {%.*s}\n", _str, eppoit.act, \
          eppoit.n, \
          eppoit.tuple.node->strlen - eppoit.n, eppoit.tuple.node->str + eppoit.n); \
    else \
      bt_log("'%s' -> ACT: %u\n", _str, eppoit.act); \
    bt_assert_int_equal(eppoit.act, _res); \
    bt_chkerr(trie_insert(trie, strlen(_str), (const uint8_t *) (_str), (_val), 1)); \
  } while(0)

#define delete(_str) \
  bt_chkerr(trie_delete(trie, strlen(_str), (const uint8_t *) (_str)))

#define find(_str, _val) \
  do { \
    e_ttuple_t e = trie_find_i(trie, strlen(_str), (const uint8_t *) (_str)); \
    bt_assert(e.ttuple.index != 0); \
    bt_assert_int_equal(e.ttuple.node->data, (_val)); \
  } while(0)


  bt_chkerr(trie_insert(trie, 9, (const uint8_t *) "foobarses", 0, 0));
  tuple = tnode_get(trie, trie->root);
  bt_assert(!tuple.node->isdata);

  /*
   * {f|oobare}
   *   [s|X]
   */

  test("f", 6, 1);
  test("foo", 7, 2);
  test("fooba", 7, 3);
  test("foobar", 6, 4);
  test("foobarse", 7, 5);
  test("foobars", 5, 6);
  test("foob", 5, 7);
  test("fo", 5, 8);
  test("g", 1, 9);
  test("b", 2, 10);

  delete("g");
  delete("b");
  delete("f");
  delete("foo");
  delete("fooba");
  delete("foobar");
  delete("foobarse");
  delete("foobars");
  delete("foob");
  delete("fo");
  delete("foobarses");

  bt_chkerr(trie_insert(trie, 9, (const uint8_t *) "foobarses", 0, 0));
  tuple = tnode_get(trie, trie->root);
  bt_assert(!tuple.node->isdata);

  /*
   * {f|oobare}
   *   [s|X]
   */

  test("foobarzeouxabracadabra", 8, 11);
  test("foobarsmeouxabracadabra", 8, 12);
  test("foobarses", 5, 13);
  test("foobares", 2, 14);
  test("foobarse", 5, 15);
  test("foobazse", 8, 16);
  test("foobarez", 1, 17);
  test("foobaren", 2, 18);
  test("foobarzillion", 8, 19);
  test("__________", 2, 20);
  test("___________", 2, 21);

  find("foobarzeouxabracadabra", 11);
  find("foobarsmeouxabracadabra", 12);
  test("foobarses", 5, 13);
  find("foobares", 14);
  find("foobarse", 15);
  find("foobazse", 16);
  find("foobarez", 17);
  find("foobaren", 18);
  find("foobarzillion", 19);
  find("__________", 20);
  find("___________", 21);

  delete("foobarzeouxabracadabra");
  delete("foobarsmeouxabracadabra");
  delete("foobares");
  delete("foobarse");
  delete("foobazse");
  delete("foobarez");
  delete("foobaren");
  delete("foobarzillion");
  delete("__________");
  delete("___________");



  // trie_print(trie, 1);

#undef test
#undef delete
#undef find

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, insert_and_find, object, "insert and find")
{
  struct trie_test * test = object;

  uint16_t  n;
  trie_t  * trie = test->trie;
  size_t    num = test->num;
  char   ** strv = test->strv;
  char    * flag = test->flag;

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    flag[j] = 1;
    if (str) {
      {
        err_r * e = trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0);
        if (e) {
          bt_log("FAIL:  '%.*s %zu/%zu'\n", (int) strlen(str), str, j, num);
          trie_print(trie, 4);
        }
        bt_chkerr(e);
      }
      {
        e_uint64_t e = trie_find(trie, strlen(str), (const uint8_t *) str);
        if (e.err) {
          bt_log("FAIL:  '%.*s %zu/%zu'\n", (int) strlen(str), str, j, num);
          trie_print(trie, 4);
        }
        bt_chkerr(e.err);
        bt_assert_int_equal(e.uint64, j);
      }
    }
  }
  bt_log("[trie] %u strings inserted\n", (unsigned) num);

  // trie_print(trie, 4);

  /* make sure all strings were properly inserted */
  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      e_uint64_t e = trie_find(trie, strlen(str), (const uint8_t *) str);
      if (e.err) {
        bt_log("FAIL: %s not found\n", str);
        trie_print(trie, 4);
      }
      bt_chkerr(e.err);
      bt_assert_int_equal(e.uint64, j);
    }
  }

  srand(1);

  /* randomly delete strings and check that only the desired were removed */
  for (size_t i = 0; i < num; i++) {
    char * str = NULL;

    do {
      n = rand() % num;
      str = strv[n];
    } while (!flag[n]);

    err_r * err = trie_delete(trie, strlen(str), (const uint8_t *) str);
    if (err) {
      bt_log("FAIL: %s not deleted (%lu)\n", str, i);
      trie_print(trie, 3);
      bt_chkerr(err);
    }

    flag[n] = 0;

    for (size_t j = 0; j < num; j++) {
      if (flag[j]) {
        str = strv[j];
        e_uint64_t e = trie_find(trie, strlen(str), (const uint8_t *) str);
        if (e.err) {
          bt_log("FAIL: %s not found (%s,%lu)\n", str, strv[n], i);
          trie_print(trie, 4);
        }
        bt_chkerr(e.err);
        bt_assert_int_equal(e.uint64, j);
      }
    }
  }
  // trie_print(trie, 3);

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, insert_delete_insert, object, "insert delete insert")
{
  struct trie_test * test = object;

  trie_t           * trie = test->trie;
  size_t             num = test->num;
  char            ** strv = test->strv;
  char             * flag = test->flag;

  // ProfilerStart("cpuprof");

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      bt_chkerr(trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0));
    }
  }
  bt_log("[trie] %u strings inserted\n", (unsigned) num);

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      e_uint64_t e = trie_find(trie, strlen(str), (const uint8_t *) str);
      bt_chkerr(e.err);
      bt_assert_int_equal(e.uint64, j);
    }
  }

  titer_t iter;

  /* ensure, that we reached every piece of data */
  for (trie_iter_init(trie, &iter); trie_iter_next(&iter);) {
    test->flag[iter.data]++;
    // printf("'%.*s' -> %zu\n", (int) iter->len, (const char *) iter->word, iter->data); fflush(stdout);
  }
  for (size_t j = 0; j < num; j++) {
    bt_assert(test->flag[j] == 1);
    test->flag[j] = 0;
  }
  trie_iter_clear(&iter);


  srand(1);

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];

    if (!flag[j] && str) {
      bt_chkerr(trie_delete(trie, strlen(str), (const uint8_t *) str));
      flag[j] = 1;
    }
  }

  srand(1);

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];
    if (flag[j] && str) {
      bt_chkerr(trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0));
      flag[j] = 0;
    }
  }

  /* no allocations happened */

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      e_uint64_t e = trie_find(trie, strlen(str), (const uint8_t *) str);
      bt_chkerr(e.err);
      bt_assert_int_equal(e.uint64, j);
    }
  }

  /* ensure, that we reached every piece of data */
  for (trie_iter_init(trie, &iter); trie_iter_next(&iter);) {
    test->flag[iter.data]++;
    // printf("'%.*s' -> %zu\n", (int) iter->len, (const char *) iter->word, iter->data); fflush(stdout);
  }
  trie_iter_clear(&iter);

  for (size_t j = 0; j < num; j++) {
    bt_assert(test->flag[j] == 1);
    test->flag[j] = 0;
  }

  // ProfilerStop();
  // trie_print(trie, 4);

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(trie, objectref)
{
  struct trie_test * test = *objectref;

  trie_clear(test->trie);

  for (size_t j = 0; j < test->num; j++) {
    free(test->strv[j]);
  }

  free(test->strv);
  free(test->flag);

  free(test);

  return BT_RESULT_OK;
}


#endif /* TEST */
