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
#ifdef TESTPROF
# include <google/profiler.h>
#endif

BT_SUITE_DEF(spman, "storage engine page manager tests");

struct spman_test {
  spman_t     pm[1];
  int         fd;
  uint32_t    num;
};

BT_TEST_DEF_PLAIN(spman, page, "page manager tests ")
{
  struct spman_test * test = malloc(sizeof(struct spman_test));

  bt_assert_ptr_not_equal(test, NULL);

  char path[] = BROOT "/src/store/spman-tests.txt";
  test->fd = open(path, O_CREAT| O_RDWR, S_IRWXU);
  if (test->fd == -1) {
    bt_log("could not open '%s'\n", path);
    return BT_RESULT_IGNORE;
  }

  test->num = 16;

  off_t size = STORE_DATASIZE * test->num;

  if (ftruncate(test->fd, size)) {
    bt_log("could not resize '%s'\n", path);
    return BT_RESULT_IGNORE;
  }

  unsigned long ps = sysconf(_SC_PAGE_SIZE);

  bt_log("[spman] STORE_DATACHUNK is %lu, memory page size is %lu\n", (long unsigned) STORE_DATACHUNK, ps);
  size_t check = STORE_DATACHUNK % (1 << 12);
  bt_assert(check == 0);

  /* format */
  {
    for (uint32_t n = 0; n < test->num; n++) {
      size_t off = STORE_DATASIZE * n;
      uint16_t * p = mmap(NULL, STORE_DATASIZE, PROT_READ  | PROT_WRITE, MAP_SHARED, test->fd, off);
      // bt_log("format page %u (mapped at %p)\n", n, (void *) p);
      bt_assert_ptr_not_equal(p, MAP_FAILED);
      memset(p, 0, STORE_DATASIZE);
      *p = STORE_DATASIZE / STORE_DATACHUNK;
      munmap(p, STORE_DATASIZE);
    }
  }

  bt_assert_ptr_not_equal(spman_init(test->pm, test->fd, 0, test->num), NULL);

  bt_log("[spman] initial store size is %u pages\n", test->pm->cnt);

  // test here...

  spman_t * pm = test->pm;

  bt_log("[spman] page load\n");
  for (uint32_t n = 0; n < test->num; n++) {
    spman_load(pm, n);
  }

  bt_log("[spman] page identity\n");
  {
    spmap_t * m1 = spman_load(pm, test->num / 2);
    void * p1 = m1->page;
    spmap_t * m2 = spman_load(pm, test->num / 2);
    void * p2 = m2->page;
    bt_assert_ptr_equal(m1, m2);
    bt_assert_ptr_equal(p1, p2);
  }

#define chunk 60
#define sgmo (STORE_SLOTSMAX * test->num)

  srid_t * vel = malloc(sizeof(srid_t) * sgmo);

  bt_log("[spman] allocate %u chunks %u bytes each\n", sgmo, chunk);
  for (uint32_t k = 0; k < sgmo; k++) {
    sdrec_t r = spman_add(pm, chunk, 0);
    bt_assert_int_not_equal(r.id, SRID_NIL);
    bt_assert(r.size >= chunk);
    bt_assert_ptr_not_equal(r.slot, NULL);
    /*bt_log("add id: %u %u, size:%u\n",
      (r.id & STORE_PAGEMASK) >> STORE_SLOTBITS,
      r.id & STORE_SLOTMASK, r.size);*/
    memset(r.slot, k % 26 + 'a', r.size);
    vel[k] = r.id;
  }

#ifdef TESTPROF
  ProfilerStart(TESTPROF);
#endif
  bt_log("[spman] burst!\n");
  for (uint32_t k = 0; k < sgmo; k++) {
    srid_t id = vel[k];
    /*bt_log("get id: %u %u\n",
      (id & STORE_PAGEMASK) >> STORE_SLOTBITS,
      id & STORE_SLOTMASK);*/
    sdrec_t r = spman_get(pm, id);
    r.map->inuse = rand() % 1000;
    bt_assert_int_equal(r.id, id);
  }
#ifdef TESTPROF
  ProfilerStop();
#endif

  free(vel);

  bt_log("[spman] final store size is %u pages\n", test->pm->cnt);

  spman_clear(test->pm);

  close(test->fd);
  free(test);

  unlink(path);

  return BT_RESULT_OK;
}

/*************************************************************************************************/

struct store_test {
  store_t s[1];
};

BT_SUITE_DEF(store, "storage engine tests");

static const char store_test_path[] = BROOT "/src/store/store-tests.txt";

BT_SUITE_SETUP_DEF(store, objectref)
{
  struct store_test * test = malloc(sizeof(struct store_test));
  bt_assert_ptr_not_equal(test, NULL);

  unlink(store_test_path);
  bt_assert_ptr_not_equal(store_init(test->s, store_test_path), NULL);

  *objectref = test;
  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_i32, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    {SKIND_INT32, NULL},
    {SKIND_INT32, NULL},
    {SKIND_INT32, NULL},
  };
  sclass_t * ctuple = store_add_class(test->s, NULL, 3, v);
  bt_assert_ptr_not_equal(ctuple, NULL);

  smrec_t * val0 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_assert_ptr_not_equal(val0, NULL);

  smrec_t * val1 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_assert_ptr_not_equal(val1, NULL);

  smrec_t * val2 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_assert_ptr_not_equal(val2, NULL);

  sdrec_t r;

  const uint8_t ccmp[] =
    "\xff\xff\xff\xff" // meta = nil
    "\x03\x00" // fcnt = 3
    "\x00\x00" "\xff\xff\xff\xff" // (SKIND_INT32, nil)
    "\x00\x00" "\xff\xff\xff\xff" // (SKIND_INT32, nil)
    "\x00\x00" "\xff\xff\xff\xff" // (SKIND_INT32, nil)
  ;

  r = spman_get(&test->s->pm, ctuple->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 24);
  bt_assert(memcmp(r.slot, ccmp, 24) == 0);

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
  ;

  r = spman_get(&test->s->pm, val0->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  r = spman_get(&test->s->pm, val1->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  r = spman_get(&test->s->pm, val2->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_u32, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    {SKIND_UINT32, NULL},
    {SKIND_UINT32, NULL},
    {SKIND_UINT32, NULL},
  };
  sclass_t * ctuple = store_add_class(test->s, NULL, 3, v);
  bt_assert_ptr_not_equal(ctuple, NULL);

  smrec_t * val0 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_assert_ptr_not_equal(val0, NULL);

  smrec_t * val1 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_assert_ptr_not_equal(val1, NULL);

  smrec_t * val2 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_assert_ptr_not_equal(val2, NULL);

  sdrec_t r;

  const uint8_t ccmp[] =
    "\xff\xff\xff\xff" // meta = nil
    "\x03\x00" // fcnt = 3
    "\x02\x00" "\xff\xff\xff\xff" // (SKIND_UINT32, nil)
    "\x02\x00" "\xff\xff\xff\xff" // (SKIND_UINT32, nil)
    "\x02\x00" "\xff\xff\xff\xff" // (SKIND_UINT32, nil)
  ;

  r = spman_get(&test->s->pm, ctuple->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 24);
  bt_assert(memcmp(r.slot, ccmp, 24) == 0);

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
  ;

  r = spman_get(&test->s->pm, val0->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  r = spman_get(&test->s->pm, val1->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  r = spman_get(&test->s->pm, val2->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 16);
  bt_assert(memcmp(r.slot, cmp, 16) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_i64, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    {SKIND_INT64, NULL},
    {SKIND_INT64, NULL},
    {SKIND_INT64, NULL},
  };
  sclass_t * ctuple = store_add_class(test->s, NULL, 3, v);
  bt_assert_ptr_not_equal(ctuple, NULL);

  smrec_t * val0 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_assert_ptr_not_equal(val0, NULL);

  smrec_t * val1 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_assert_ptr_not_equal(val1, NULL);

  smrec_t * val2 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_assert_ptr_not_equal(val2, NULL);

  sdrec_t r;

  const uint8_t ccmp[] =
    "\xff\xff\xff\xff" // meta = nil
    "\x03\x00" // fcnt = 3
    "\x01\x00" "\xff\xff\xff\xff" // (SKIND_INT64, nil)
    "\x01\x00" "\xff\xff\xff\xff" // (SKIND_INT64, nil)
    "\x01\x00" "\xff\xff\xff\xff" // (SKIND_INT64, nil)
  ;

  r = spman_get(&test->s->pm, ctuple->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 24);
  bt_assert(memcmp(r.slot, ccmp, 24) == 0);

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
  ;

  r = spman_get(&test->s->pm, val0->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  r = spman_get(&test->s->pm, val1->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  r = spman_get(&test->s->pm, val2->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_u64, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    {SKIND_UINT64, NULL},
    {SKIND_UINT64, NULL},
    {SKIND_UINT64, NULL},
  };
  sclass_t * ctuple = store_add_class(test->s, NULL, 3, v);
  bt_assert_ptr_not_equal(ctuple, NULL);

  smrec_t * val0 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_assert_ptr_not_equal(val0, NULL);

  smrec_t * val1 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_assert_ptr_not_equal(val1, NULL);

  smrec_t * val2 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_assert_ptr_not_equal(val2, NULL);

  sdrec_t r;

  const uint8_t ccmp[] =
    "\xff\xff\xff\xff" // meta = nil
    "\x03\x00" // fcnt = 3
    "\x03\x00" "\xff\xff\xff\xff" // (SKIND_UINT64, nil)
    "\x03\x00" "\xff\xff\xff\xff" // (SKIND_UINT64, nil)
    "\x03\x00" "\xff\xff\xff\xff" // (SKIND_UINT64, nil)
  ;

  r = spman_get(&test->s->pm, ctuple->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 24);
  bt_assert(memcmp(r.slot, ccmp, 24) == 0);

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
  ;

  r = spman_get(&test->s->pm, val0->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  r = spman_get(&test->s->pm, val1->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  r = spman_get(&test->s->pm, val2->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 28);
  bt_assert(memcmp(r.slot, cmp, 28) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_str, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    {SKIND_STRING, NULL},
    {SKIND_STRING, NULL},
    {SKIND_STRING, NULL},
  };
  sclass_t * ctuple = store_add_class(test->s, NULL, 3, v);
  bt_assert_ptr_not_equal(ctuple, NULL);

  smrec_t * val0 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_assert_ptr_not_equal(val0, NULL);

  smrec_t * val1 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_assert_ptr_not_equal(val1, NULL);

  smrec_t * val2 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_assert_ptr_not_equal(val2, NULL);

  sdrec_t r;

  const uint8_t ccmp[] =
    "\xff\xff\xff\xff" // meta = nil
    "\x03\x00" // fcnt = 3
    "\x05\x00" "\xff\xff\xff\xff" // (SKIND_STRING, nil)
    "\x05\x00" "\xff\xff\xff\xff" // (SKIND_STRING, nil)
    "\x05\x00" "\xff\xff\xff\xff" // (SKIND_STRING, nil)
  ;

  r = spman_get(&test->s->pm, ctuple->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 24);
  bt_assert(memcmp(r.slot, ccmp, 24) == 0);

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\x08\x00" "AAAAAAAA"
    "\x08\x00" "BBBBBBBB"
    "\x08\x00" "CCCCCCCC"
  ;

  r = spman_get(&test->s->pm, val0->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 34);
  bt_assert(memcmp(r.slot, cmp, 34) == 0);

  r = spman_get(&test->s->pm, val1->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 34);
  bt_assert(memcmp(r.slot, cmp, 34) == 0);

  r = spman_get(&test->s->pm, val2->id);
  bt_assert_int_not_equal(r.id, SRID_NIL);
  bt_assert_int_equal(r.size, 34);
  bt_assert(memcmp(r.slot, cmp, 34) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, simple, object, "simple tests for the storage engine")
{
  struct store_test * test = object;

  {
    sclass_t * ctuple;
    {
      scfld_t v[] = {
        {SKIND_OBJECT, NULL},
        {SKIND_OBJECT, NULL},
        {SKIND_OBJECT, NULL},
      };
      ctuple = store_add_class(test->s, NULL, 3, v);
    }
    bt_assert_ptr_not_equal(ctuple, NULL);

    sclass_t * cvalue;
    {
      scfld_t v[] = {
        {SKIND_UINT32, NULL},
      };
      cvalue = store_add_class(test->s, NULL, 1, v);
    }
    bt_assert_ptr_not_equal(cvalue, NULL);

    srid_t v[100];
    for (uint16_t k = 0; k < 2; k++) {
      smrec_t * v0 = store_add_object(test->s, cvalue, k);
      bt_assert_ptr_not_equal(v0, NULL);
      smrec_t * v1 = store_add_object(test->s, cvalue, k * 2);
      bt_assert_ptr_not_equal(v1, NULL);
      smrec_t * v2 = store_add_object(test->s, cvalue, k * 3);
      bt_assert_ptr_not_equal(v2, NULL);
      smrec_t * r = store_add_object(test->s, ctuple, v0, v1, v2);
      bt_assert_ptr_not_equal(r, NULL);
      v[k] = r->id;
    }

    bt_log("[store] force discard\n"); gc_collect(&test->s->g, 1);

    for (uint16_t k = 0; k < 2; k++) {
      smrec_t * r = store_get_object(test->s, v[k]);
      bt_assert_ptr_not_equal(r, NULL);
      bt_assert_int_equal(v[k], r->id);
    }
  }

  bt_log("[store] force discard\n"); gc_collect(&test->s->g, 1);

  {
    sclass_t * clist;
    {
      scfld_t v[] = {
        {SKIND_UINT32, NULL},
        {SKIND_OBJECT, NULL},
      };
      clist = store_add_class(test->s, NULL, 2, v);
    }
    bt_assert_ptr_not_equal(clist, NULL);

    smrec_t * head = NULL;
    for (uint16_t k = 0; k < 10; k++) {
      smrec_t * r = store_add_object(test->s, clist, k, head);
      bt_assert_ptr_not_equal(r, NULL);
      head = r;
    }

    srid_t hid = head->id;

    bt_log("[store] force discard\n"); gc_collect(&test->s->g, 1);
    head = store_get_object(test->s, hid);
    bt_assert_ptr_not_equal(head, NULL);
    bt_assert_int_equal(head->id, hid);
  }

  bt_log("[store] force discard\n"); gc_collect(&test->s->g, 1);

  {
    sclass_t * clist;
    {
      scfld_t v[] = {
        {SKIND_OBJECT, NULL},
        {SKIND_STRING, NULL},
      };
      clist = store_add_class(test->s, NULL, 2, v);
    }
    bt_assert_ptr_not_equal(clist, NULL);

    smrec_t * head = NULL;
    char buf[128];
    for (uint16_t k = 0; k < 10; k++) {
      snprintf(buf, 128, "AAA %u", k);
      smrec_t * r = store_add_object(test->s, clist, head, (int) strlen(buf), buf);
      bt_assert_ptr_not_equal(r, NULL);
      head = r;
    }

    srid_t hid = head->id;

    bt_log("[store] reopen\n");
    store_clear(test->s);
    bt_assert_ptr_not_equal(store_init(test->s, store_test_path), NULL);

    head = store_get_object(test->s, hid);
    bt_assert_ptr_not_equal(head, NULL);
    bt_assert_int_equal(head->id, hid);
  }

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(store, objectref)
{
  struct store_test * test = *objectref;

  store_clear(test->s);

  free(test);

  unlink(store_test_path);

  return BT_RESULT_OK;
}

#endif /* TEST */
