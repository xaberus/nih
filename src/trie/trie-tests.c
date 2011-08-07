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
  trie_t      trie[1];
  gc_global_t g[1];
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

  mem_allocator_t a;
  a.realloc = plain_realloc;
  a.ud = NULL;

  gc_init(test->g, a);

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

  bt_assert_ptr_not_equal(trie_init(test->g, test->trie, 4096), NULL);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, str, object, "str mode")
{
  struct trie_test * test = object;
  trie_t           * trie = test->trie;
  err_t              err;
  struct trie_eppoit eppoit;
  struct tnode_tuple tuple;
  struct tnode_iter  iter[1] = {tnode_iter(trie->nodes, 0)};

#define test(_str, _res) \
  eppoit = _trie_insert_decide(trie, tuple, iter, strlen(_str), (const uint8_t *) (_str), 1); \
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
  err = trie_insert(trie, strlen(_str), (const uint8_t *) (_str), 0, 1); \
  bt_chkerr(err)

#define delete(_str) \
  err = trie_delete(trie, strlen(_str), (const uint8_t *) (_str)); \
  bt_chkerr(err)

#define find(_str) \
  tuple = trie_find_i(trie, strlen(_str), (const uint8_t *) (_str)); \
  bt_assert(tuple.index != 0)


  err = trie_insert(trie, 9, (const uint8_t *) "foobarses", 0, 0);
  bt_chkerr(err);
  tuple = tnode_iter_get(iter, trie->root);
  bt_assert(!tuple.node->isdata);

  /*
   * {f|oobare}
   *   [s|X]
   */

  test("f", 6);
  test("foo", 7);
  test("fooba", 7);
  test("foobar", 6);
  test("foobarse", 7);
  test("foobars", 5);
  test("foob", 5);
  test("fo", 5);
  test("g", 1);
  test("b", 2);

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

  err = trie_insert(trie, 9, (const uint8_t *) "foobarses", 0, 0);
  bt_chkerr(err);
  tuple = tnode_iter_get(iter, trie->root);
  bt_assert(!tuple.node->isdata);

  /*
   * {f|oobare}
   *   [s|X]
   */

  test("foobarzeouxabracadabra", 8);
  test("foobarsmeouxabracadabra", 8);
  test("foobarses", 5);
  test("foobares", 2);
  test("foobarse", 5);
  test("foobazse", 8);
  test("foobarez", 1);
  test("foobaren", 2);
  test("foobarzillion", 8);
  test("__________", 2);
  test("___________", 2);

  find("foobarzeouxabracadabra");
  find("foobarsmeouxabracadabra");
  find("foobares");
  find("foobarse");
  find("foobazse");
  find("foobarez");
  find("foobaren");
  find("foobarzillion");
  find("__________");
  find("___________");

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

  uint16_t           n;
  uint64_t           data;
  err_t              err;

  trie_t           * trie = test->trie;
  size_t             num = test->num;
  char            ** strv = test->strv;
  char             * flag = test->flag;

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    flag[j] = 1;
    if (str) {
      err = trie_insert(trie, strlen(str), (const uint8_t *) str, j, 0);
      if (err) {
        bt_log("FAIL:  '%.*s %zu/%zu'\n", (int) strlen(str), str, j, num);
        trie_print(trie, 4);
      }
      bt_chkerr(err);
    }
  }

  // trie_print(trie, 4);

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
      if (err) {
        bt_log("FAIL: %s not found\n", str);
        trie_print(trie, 4);
      }
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  srand(1);

  for (size_t i = 0; i < num; i++) {
    char * str = NULL;

    do {
      n = rand() % num;
      str = strv[n];
    } while (!flag[n]);

    err = trie_delete(trie, strlen(str), (const uint8_t *) str);
    if (err) {
      bt_log("FAIL: %s not deleted (%lu)\n", str, i);
      trie_print(trie, 3);
      bt_chkerr(err);
    }

    flag[n] = 0;

    for (size_t j = 0; j < num; j++) {
      if (flag[j]) {
        str = strv[j];
        err = trie_find(trie, strlen(str), (const uint8_t *) str, &data);
        if (err) {
          bt_log("FAIL: %s not found (%s,%lu)\n", str, strv[n], i);
          trie_print(trie, 4);
          bt_chkerr(err);
        }
        bt_assert_int_equal(data, j);
      }
    }
  }
  // trie_print(trie, 3);

  return BT_RESULT_OK;
}

BT_TEST_DEF(trie, insert_delete_insert, object, "insert delete insert")
{
  struct trie_test * test = object;

  uint64_t           data;
  err_t              err;
  trie_t           * trie = test->trie;
  size_t             num = test->num;
  char            ** strv = test->strv;
  char             * flag = test->flag;

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

  trie_iter_t iterstor[1], * iter;

  /* ensure, that we reached every piece of data */
  for (iter = trie_iter_init(trie, iterstor); iter && trie_iter_next(iter);) {
    test->flag[iter->data]++;
    // printf("'%.*s' -> %zu\n", (int) iter->len, (const char *) iter->word, iter->data); fflush(stdout);
  }
  for (size_t j = 0; j < num; j++) {
    bt_assert(test->flag[j] == 1);
    test->flag[j] = 0;
  }

  trie_iter_clear(iter);


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

  gc_clear(test->g);

  free(test);

  return BT_RESULT_OK;
}


#endif /* TEST */
