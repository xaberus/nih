#ifdef TEST
# include <bt.h>

/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/


static inline
void sx_pool_print(struct sx_pool * pool)
{
  struct chunk_header * c;
  struct data_header  * d;
  int                   count;

  bt_log("{\n");
  for (c = pool->tail; c; c = c->prev) {
    bt_log(" (s: %zu)", c->size);
    count = 0;
    for (d = sx_chunk_first_data(c); d; d = sx_chunk_next_data(c, d)) {
      if ((count) % 8 == 0)
        bt_log("\n  ");
      bt_log("[s: %zu%s%s]", d->size, 
          d->flags & SX_POOL_DATA_FLAG_CACHE ? " c" : "",
          d->flags & SX_POOL_DATA_FLAG_FREE ? " F" : "");
      count++;
    }
    bt_log("\n");
  }
  bt_log("}\n");
}

static inline
void sx_pool_print_cache(struct sx_pool * pool)
{

  bt_log("{(cache)\n");
  for (unsigned int i = 0; i < SX_POOL_FCACHE_UBITS_MAX; i++) {
    if (pool->fcache[i].size)
      bt_log("  [s: %u] * %zu\n", ((i+1)<<4), pool->fcache[i].size);
  }
  bt_log("}\n");

}


BT_SUITE_DEF(sx_util, "s-expression utils tests");

# define N 32
# define M 128
BT_TEST_DEF_PLAIN(sx_util, sx_pool, "sx_pool")
{
  struct sx_pool * pool = malloc(sizeof(struct sx_pool));
  char           * arr[N];
  char           * barr[M];
  unsigned int k;

  if (!pool)
    return BT_RESULT_FAIL;

  sx_pool_init(pool);

#define TEST_C
#define TEST_S
#define TEST_B
#define TEST_M

#ifdef TEST_C
  bt_log(">> allocate some datums\n"); k = 0;
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 32);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 48);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 48);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 64);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);

  bt_log(">> free them\n");
  for (unsigned int i = 0; i < k; i++)
    sx_pool_retmem(pool, arr[i]);

  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> allocate same datums again\n"); k = 0;
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 32);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 48);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 48);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 64);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);
  arr[k] = sx_pool_getmem(pool, 16);
  bt_assert_ptr_not_equal(arr[k++], NULL);

  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);
#endif
#ifdef TEST_S
  bt_log(">> allocate %u datums\n", N);
  for (unsigned int i = 0; i < N; i++) {
    arr[i] = sx_pool_getmem(pool, 256);
    bt_assert_ptr_not_equal(arr[i], NULL);
    memset(arr[i], 0, N);
  }
  //sx_pool_print(pool);

  bt_log(">> free some datums\n");
  for (unsigned int i = 0; i < N; i++) {
    if (i % 4 && i!=6)
      sx_pool_retmem(pool, arr[i]);
  }
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> normalize pool\n");
  sx_pool_normalize(pool);
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);
  bt_log(">> allocate 3 datums\n");
  arr[0] = sx_pool_getmem(pool, 256);
  bt_assert_ptr_not_equal(arr[0], NULL);
  arr[4] = sx_pool_getmem(pool, 256);
  bt_assert_ptr_not_equal(arr[0], NULL);
  arr[6] = sx_pool_getmem(pool, 256);
  bt_assert_ptr_not_equal(arr[0], NULL);
  arr[8] = sx_pool_getmem(pool, 256);
  bt_assert_ptr_not_equal(arr[0], NULL);
  arr[12] = sx_pool_getmem(pool, 256);
  bt_assert_ptr_not_equal(arr[0], NULL);
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> free %u datums & normalize pool\n", 2);
  sx_pool_retmem(pool, arr[3]);
  sx_pool_retmem(pool, arr[5]);
  sx_pool_normalize(pool);
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> clear pool\n");
  sx_pool_clear(pool);

  bt_log(">> allocate big datum\n");
  arr[0] = sx_pool_getmem(pool, 4032);
  bt_assert_ptr_not_equal(arr[0], NULL);
  //sx_pool_print(pool);

  bt_log(">> allocate _really_ big datum\n");
  arr[0] = sx_pool_getmem(pool, 65536);
  bt_assert_ptr_not_equal(arr[0], NULL);
  //sx_pool_print(pool);

  bt_log(">> clear pool\n");
  sx_pool_clear(pool);

  bt_log(">> allocate corner\n");
  arr[0] = sx_pool_getmem(pool, 4001);
  bt_assert_ptr_not_equal(arr[0], NULL);
  sx_pool_normalize(pool);
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> clear pool\n");
  sx_pool_clear(pool);
#endif
#ifdef TEST_B
  bt_log(">> burst allocate \n");
  memset(barr, 0, M * sizeof(char *));
  for (unsigned int i = 0, j = 0, k=0xffffffff, h = 0xbe8de71f; i < 1024; i++) {
    j = (h + ((i^h) + 0xff))%M;
    if (barr[j]) {
      sx_pool_retmem(pool, barr[j]);
      barr[j] = NULL;
    } else {
      while(!(h = ((((h) + 0xaaaaaaaa) ^ 0xeda726ae) & (k-- & 0x3ff)) + 1));
      barr[j] = sx_pool_getmem(pool, h);
      bt_assert_ptr_not_equal(barr[j], NULL);
    }
  }
  //sx_pool_print(pool);
  //sx_pool_print_cache(pool);

  bt_log(">> clear pool\n");
  sx_pool_clear(pool);
#endif
#ifdef TEST_M
  bt_log(">> grow allocate \n");
  for (unsigned int i = 16; i < 4096*4; i+=16) {
    barr[0] = sx_pool_getmem(pool, i);
    bt_assert_ptr_not_equal(barr[0], NULL);
    sx_pool_retmem(pool, barr[0]);
  }
  sx_pool_normalize(pool);
  //sx_pool_print(pool);

  bt_log(">> clear pool\n");
  sx_pool_clear(pool);
#endif
  bt_log(">> clear pool\n");
  sx_pool_clear(pool);

  free(pool);

  return BT_RESULT_OK;
}


#endif /* TEST */
