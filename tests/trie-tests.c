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

BT_TEST_DEF_PLAIN(trie, insert_and_find, "insert and find")
{
  struct trie trie;

  trie_init(&trie);
  char        buf[128];
  FILE      * fp;
  uint16_t    n;
  uint64_t    data;
  err_t       err;

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  if (!fp)
    return BT_RESULT_IGNORE;

  while (fgets(buf, 512, fp)) {
    n++;
  }
  fclose(fp);

  size_t num = n;
  char * strv[num];

  // int fd;

  memset(&strv, 0, sizeof(char *) * num);

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  while (n < num && fgets(buf, 512, fp)) {
    strv[n++] = strdup(buf);
  }
  fclose(fp);

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      // bt_log("INS '%.*s' -> %d\n", (int)strlen(buf)-1,  buf, n);
      err = trie_insert(&trie, (const uint8_t *) str, strlen(str) - 1, j);
      if (err.composite)
        bt_log("FAIL:  '%.*s'\n", (int) strlen(str) - 1, str);
      bt_chkerr(err);
    }
  }

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(&trie, (const uint8_t *) str, strlen(str) - 1, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  srand(1);

  // fd = open("/tmp/dot/000", O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
  // trie_print(&trie, fd);
  // close(fd);


  for (size_t i = 0; i < num; i++) {
    char * str = NULL;

    // snprintf(buf, 512, "/tmp/dot/%03zd", i+1);


    while (!str) {
      n = rand() % num;
      str = strv[n];
    }

    err = trie_delete(&trie, (const uint8_t *) str, strlen(str) - 1);
    bt_chkerr(err);

    // bt_log("log '%s'\n", buf);
    // fd = open(buf, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR);
    // bt_assert(fd != -1);
    // trie_print(&trie, fd);
    // close(fd);

    free(str);
    strv[n] = NULL;

    for (size_t j = 0; j < num; j++) {
      str = strv[j];
      if (str) {
        // bt_log("FND '%.*s' == %d\n", (int)strlen(str)-1,  str, j);
        err = trie_find(&trie, (const uint8_t *) str, strlen(str) - 1, &data);
        bt_chkerr(err);
        bt_assert_int_equal(data, j);
      }
    }
  }

  trie_clear(&trie);

  return BT_RESULT_OK;
}

BT_TEST_DEF_PLAIN(trie, insert_delete_insert, "insert delete insert")
{
  struct trie trie;

  trie_init(&trie);
  char        buf[128];
  FILE      * fp;
  uint16_t    n;
  uint64_t    data;
  err_t       err;

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  if (!fp)
    return BT_RESULT_IGNORE;

  while (fgets(buf, 512, fp)) {
    n++;
  }
  fclose(fp);

  size_t   num = n;
  uint32_t s;
  char   * strv[num];
  char     flag[num];

  memset(&strv, 0, sizeof(char *) * num);
  memset(&flag, 0, num);

  n = 0;
  fp = fopen("tests/trie-tests.txt", "r");
  while (n < num && fgets(buf, 512, fp)) {
    strv[n++] = strdup(buf);
  }
  fclose(fp);

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_insert(&trie, (const uint8_t *) str, strlen(str) - 1, j);
      if (err.composite)
        bt_log("FAIL:  '%.*s'\n", (int) strlen(str) - 1, str);
      bt_chkerr(err);
    }
  }

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(&trie, (const uint8_t *) str, strlen(str) - 1, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  srand(1);

  s = trie.size;

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];

    if (!flag[j] && str) {
      err = trie_delete(&trie, (const uint8_t *) str, strlen(str) - 1);
      bt_chkerr(err);
      flag[j] = 1;
    }
  }

  srand(1);

  for (size_t i = 0, j; i < num / 2; i++) {
    j = rand() % num;
    char * str = strv[j];
    if (flag[j] && str) {
      err = trie_insert(&trie, (const uint8_t *) str, strlen(str) - 1, j);
      if (err.composite)
        bt_log("FAIL:  '%.*s'\n", (int) strlen(str) - 1, str);
      bt_chkerr(err);
      flag[j] = 0;
    }
  }

  /* no allocations happened */
  bt_assert_int_equal(s, trie.size);

  for (size_t j = 0; j < num; j++) {
    char * str = strv[j];
    if (str) {
      err = trie_find(&trie, (const uint8_t *) str, strlen(str) - 1, &data);
      bt_chkerr(err);
      bt_assert_int_equal(data, j);
    }
  }

  for (size_t j = 0; j < num; j++) {
    free(strv[j]);
  }

  trie_clear(&trie);

  return BT_RESULT_OK;
}


#endif /* TEST */
