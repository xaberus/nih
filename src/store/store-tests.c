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
#include <stddef.h>
#ifdef TESTPROF
# include <google/profiler.h>
#endif

#include "store/sdisk.h"

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
    printf("could not open '%s'\n", path);
    return BT_RESULT_IGNORE;
  }

  test->num = 16;

  /*off_t size = STORE_DATASIZE * test->num;

  if (ftruncate(test->fd, size)) {
    printf("could not resize '%s'\n", path);
    return BT_RESULT_IGNORE;
  }*/

  unsigned long ps = sysconf(_SC_PAGE_SIZE);

  printf("[spman] STORE_DATACHUNK is %lu, memory page size is %lu\n", (long unsigned) STORE_DATACHUNK, ps);
  size_t check = STORE_DATACHUNK % (1 << 12);
  bt_assert(check == 0);

  printf("[spman] STORE_SLOTSMAX is %u, new pages size is %u\n",
    STORE_SLOTSMAX, STORE_DATACHUNK * STORE_NEWPN);

  /* format */
  /*{
    for (uint32_t n = 0; n < test->num; n++) {
      size_t off = STORE_DATASIZE * n;
      uint16_t * p = mmap(NULL, STORE_DATASIZE, PROT_READ  | PROT_WRITE, MAP_SHARED, test->fd, off);
      // printf("format page %u (mapped at %p)\n", n, (void *) p);
      bt_assert_ptr_not_equal(p, MAP_FAILED);
      memset(p, 0, STORE_DATASIZE);
      *p = STORE_DATASIZE / STORE_DATACHUNK;
      munmap(p, STORE_DATASIZE);
    }
  }*/

  bt_chkerr(spman_init(test->pm, test->fd, 0, 0));
  spman_t * pm = test->pm;

#define chunk 60
#define sgmo ((STORE_SLOTSMAX * test->num) - 1)

  printf("[spman] initial store size is %u pages\n", pm->cnt);

  // test here...

  srid_t * vel = malloc(sizeof(srid_t) * sgmo);

  printf("[spman] allocate %u chunks %u bytes each\n", sgmo, chunk);
  for (uint32_t k = 0; k < sgmo; k++) {
    e_sdrec_t e = spman_add(pm, chunk, 0); bt_chkerr(e.err);
    bt_assert_int_not_equal(e.sdrec.id, SRID_NIL);
    bt_assert(e.sdrec.size >= chunk);
    bt_assert_ptr_not_equal(e.sdrec.slot, NULL);
    /*printf("add id: %u %u, size:%u\n",
      (e.sdrec.id & STORE_PAGEMASK) >> STORE_SLOTBITS,
      e.sdrec.id & STORE_SLOTMASK, e.sdrec.size);*/
    memset(e.sdrec.slot, k % 26 + 'a', e.sdrec.size);
    vel[k] = e.sdrec.id;
  }

  printf("[spman] page identity\n");
  {
    e_spmap_t e1 = spman_load(pm, test->num / 2); bt_chkerr(e1.err);
    e_spmap_t e2 = spman_load(pm, test->num / 2); bt_chkerr(e1.err);
    bt_assert_ptr_equal(e1.spmap, e2.spmap);
    bt_assert_ptr_equal(e1.spmap->page, e2.spmap->page);
  }

#ifdef TESTPROF
  ProfilerStart(TESTPROF);
#endif
  printf("[spman] burst!\n");
  for (uint32_t k = 0; k < sgmo; k++) {
    srid_t id = vel[k];
    /*printf("get id: %u %u\n",
      (id & STORE_PAGEMASK) >> STORE_SLOTBITS,
      id & STORE_SLOTMASK);*/
    e_sdrec_t e = spman_get(pm, id);
    e.sdrec.map->inuse = rand() % 1000;
    bt_assert_int_equal(e.sdrec.id, id);
  }
#ifdef TESTPROF
  ProfilerStop();
#endif

  free(vel);

  printf("[spman] final store size is %u pages\n", test->pm->cnt);

  spman_clear(test->pm);

  close(test->fd);
  free(test);

  unlink(path);

  bt_chkerr(err_pop());

  return BT_RESULT_OK;
}

/*************************************************************************************************/

struct store_test {
  store_t s[1];
};

#define SCFLDDEF(_k, _c) (scfld_t){(_k), (_c), NULL, 0}

BT_SUITE_DEF(store, "storage engine tests");

static const char store_test_path[] = BROOT "/src/store/store-tests.txt";

BT_SUITE_SETUP_DEF(store, objectref)
{
  struct store_test * test = malloc(sizeof(struct store_test));
  bt_assert_ptr_not_equal(test, NULL);

  unlink(store_test_path);
  bt_chkerr(store_init(test->s, store_test_path));

  *objectref = test;
  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_i32, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_INT32, NULL),
    SCFLDDEF(SKIND_INT32, NULL),
    SCFLDDEF(SKIND_INT32, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

  e_smrec_t val0 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_chkerr(val0.err);

  e_smrec_t val1 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_chkerr(val1.err);

  e_smrec_t val2 = store_add_object(test->s, ctuple,
    -559038737, -559038737, -559038737
  );
  bt_chkerr(val2.err);

	e_sdrec_t er;
	{
		uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);  // class meta = nil
    c = sdisk_u16_write(c, 3);         // field count = 3

    c = sdisk_u16_write(c, SKIND_INT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_INT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_INT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);

		er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
		bt_assert_int_equal(er.sdrec.size, 36);
		bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
	}

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
  ;

  er = spman_get(&test->s->pm, val0.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  er = spman_get(&test->s->pm, val1.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  er = spman_get(&test->s->pm, val2.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  return BT_RESULT_OK;
}

#if 1
BT_TEST_DEF(store, type_u32, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_UINT32, NULL),
    SCFLDDEF(SKIND_UINT32, NULL),
    SCFLDDEF(SKIND_UINT32, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

  e_smrec_t val0 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_chkerr(val0.err);

  e_smrec_t val1 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_chkerr(val1.err);

  e_smrec_t val2 = store_add_object(test->s, ctuple,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef
  );
  bt_chkerr(val2.err);

	e_sdrec_t er;
	{
		uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);  // class meta = nil
    c = sdisk_u16_write(c, 3);         // field count = 3

    c = sdisk_u16_write(c, SKIND_UINT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_UINT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_UINT32);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);

		er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
		bt_assert_int_equal(er.sdrec.size, 36);
		bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
	}

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
    "\xef\xbe\xad\xde"
  ;

  er = spman_get(&test->s->pm, val0.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  er = spman_get(&test->s->pm, val1.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  er = spman_get(&test->s->pm, val2.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 16);
  bt_assert(memcmp(er.sdrec.slot, cmp, 16) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_i64, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_INT64, NULL),
    SCFLDDEF(SKIND_INT64, NULL),
    SCFLDDEF(SKIND_INT64, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

  e_smrec_t val0 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_chkerr(val0.err);

  e_smrec_t val1 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_chkerr(val1.err);

  e_smrec_t val2 = store_add_object(test->s, ctuple,
    -5764947276981223697, -5764947276981223697, -5764947276981223697
  );
  bt_chkerr(val2.err);

	e_sdrec_t er;
	{
		uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);  // class meta = nil
    c = sdisk_u16_write(c, 3);         // field count = 3

    c = sdisk_u16_write(c, SKIND_INT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_INT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_INT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);

		er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
		bt_assert_int_equal(er.sdrec.size, 36);
		bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
	}

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
    "\xef\xbe\xad\xde\xfe\xca\xfe\xaf"
  ;

  er = spman_get(&test->s->pm, val0.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  er = spman_get(&test->s->pm, val1.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  er = spman_get(&test->s->pm, val2.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_u64, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_UINT64, NULL),
    SCFLDDEF(SKIND_UINT64, NULL),
    SCFLDDEF(SKIND_UINT64, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

  e_smrec_t val0 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_chkerr(val0.err);

  e_smrec_t val1 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_chkerr(val1.err);

  e_smrec_t val2 = store_add_object(test->s, ctuple,
    0xdeadbeefaffecafe, 0xdeadbeefaffecafe, 0xdeadbeefaffecafe
  );
  bt_chkerr(val2.err);

	e_sdrec_t er;
	{
		uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);  // class meta = nil
    c = sdisk_u16_write(c, 3);         // field count = 3

    c = sdisk_u16_write(c, SKIND_UINT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_UINT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_UINT64);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);

		er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
		bt_assert_int_equal(er.sdrec.size, 36);
		bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
	}

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
    "\xfe\xca\xfe\xaf\xef\xbe\xad\xde"
  ;

  er = spman_get(&test->s->pm, val0.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  er = spman_get(&test->s->pm, val1.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  er = spman_get(&test->s->pm, val2.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 28);
  bt_assert(memcmp(er.sdrec.slot, cmp, 28) == 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, type_str, object, "type tests for the storage engine")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_STRING, NULL),
    SCFLDDEF(SKIND_STRING, NULL),
    SCFLDDEF(SKIND_STRING, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

  e_smrec_t val0 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_chkerr(val0.err);

  e_smrec_t val1 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_chkerr(val1.err);

  e_smrec_t val2 = store_add_object(test->s, ctuple,
    8, "AAAAAAAA",
    8, "BBBBBBBB",
    8, "CCCCCCCC"
  );
  bt_chkerr(val2.err);

	e_sdrec_t er;
	{
		uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);  // class meta = nil
    c = sdisk_u16_write(c, 3);         // field count = 3

    c = sdisk_u16_write(c, SKIND_STRING);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_STRING);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_u16_write(c, SKIND_STRING);
		c = sdisk_rid_write(c, SRID_NIL);
		c = sdisk_rid_write(c, SRID_NIL);

		er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
		bt_assert_int_equal(er.sdrec.size, 36);
		bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
	}

  const uint8_t cmp[] =
    "\x00\x00\x00\x00" // class = 0
    "\x08\x00" "AAAAAAAA"
    "\x08\x00" "BBBBBBBB"
    "\x08\x00" "CCCCCCCC"
  ;

  er = spman_get(&test->s->pm, val0.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 34);
  bt_assert(memcmp(er.sdrec.slot, cmp, 34) == 0);

  er = spman_get(&test->s->pm, val1.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 34);
  bt_assert(memcmp(er.sdrec.slot, cmp, 34) == 0);

  er = spman_get(&test->s->pm, val2.smrec->id);  bt_chkerr(er.err);
  bt_assert_int_equal(er.sdrec.size, 34);
  bt_assert(memcmp(er.sdrec.slot, cmp, 34) == 0);

  return BT_RESULT_OK;
}
#endif

BT_TEST_DEF(store, simple, object, "simple tests for the storage engine")
{
  struct store_test * test = object;

  {
    sclass_t * ctuple;
    {
      scfld_t v[] = {
        SCFLDDEF(SKIND_OBJECT, NULL),
        SCFLDDEF(SKIND_OBJECT, NULL),
        SCFLDDEF(SKIND_OBJECT, NULL),
      };
      e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
      ctuple = e.sclass;
    }

    sclass_t * cvalue;
    {
      scfld_t v[] = {
        SCFLDDEF(SKIND_UINT32, NULL),
      };
      e_sclass_t e = store_add_class(test->s, NULL, 1, v); bt_chkerr(e.err);
      cvalue = e.sclass;
    }

    srid_t v[100];
    for (uint16_t k = 0; k < 2; k++) {
      e_smrec_t v0 = store_add_object(test->s, cvalue, k); bt_chkerr(v0.err);
      e_smrec_t v1 = store_add_object(test->s, cvalue, k * 2); bt_chkerr(v1.err);
      e_smrec_t v2 = store_add_object(test->s, cvalue, k * 3); bt_chkerr(v2.err);
      e_smrec_t r = store_add_object(test->s, ctuple, v0, v1, v2); bt_chkerr(r.err);
      v[k] = r.smrec->id;
    }

    printf("[store] force discard\n"); gc_collect(&test->s->g, 1);

    for (uint16_t k = 0; k < 2; k++) {
      e_smrec_t e = store_get_object(test->s, v[k]); bt_chkerr(e.err);
      bt_assert_int_equal(v[k], e.smrec->id);
    }
  }

  printf("[store] force discard\n"); gc_collect(&test->s->g, 1);

  {
    sclass_t * clist;
    {
      scfld_t v[] = {
        SCFLDDEF(SKIND_UINT32, NULL),
        SCFLDDEF(SKIND_OBJECT, NULL),
      };
      e_sclass_t e = store_add_class(test->s, NULL, 2, v); bt_chkerr(e.err);
      clist = e.sclass;
    }

    smrec_t * head = NULL;
    for (uint16_t k = 0; k < 10; k++) {
      e_smrec_t e = store_add_object(test->s, clist, k, head); bt_chkerr(e.err);
      head = e.smrec;
    }

    srid_t hid = head->id;

    printf("[store] force discard\n"); gc_collect(&test->s->g, 1);
    e_smrec_t e = store_get_object(test->s, hid); bt_chkerr(e.err);
    bt_assert_int_equal(e.smrec->id, hid);
  }

  printf("[store] force discard\n"); gc_collect(&test->s->g, 1);

  {
    sclass_t * clist;
    {
      scfld_t v[] = {
        SCFLDDEF(SKIND_OBJECT, NULL),
        SCFLDDEF(SKIND_STRING, NULL),
      };
      e_sclass_t e = store_add_class(test->s, NULL, 2, v); bt_chkerr(e.err);
      clist = e.sclass;
    }

    smrec_t * head = NULL;
    char buf[128];
    for (uint16_t k = 0; k < 10; k++) {
      snprintf(buf, 128, "AAA %u", k);
      e_smrec_t e = store_add_object(test->s, clist, head, (int) strlen(buf), buf); bt_chkerr(e.err);
      head = e.smrec;
    }

    srid_t hid = head->id;

    printf("[store] reopen\n");
    store_clear(test->s);
    bt_chkerr(store_init(test->s, store_test_path));

    e_smrec_t e = store_get_object(test->s, hid); bt_chkerr(e.err);
    bt_assert_int_equal(e.smrec->id, hid);
  }

  return BT_RESULT_OK;
}

BT_TEST_DEF(store, defer, object, "tests for deferred object references")
{
  struct store_test * test = object;

  scfld_t v[] = {
    SCFLDDEF(SKIND_UINT32, NULL),
    SCFLDDEF(SKIND_ODREF, NULL),
    SCFLDDEF(SKIND_OBJECT, NULL),
  };
  e_sclass_t e = store_add_class(test->s, NULL, 3, v); bt_chkerr(e.err);
  sclass_t * ctuple = e.sclass;

//#define NN 10000
#define NN 5000

  srand(777);

  smrec_t * crosslink[NN] = {NULL};

  srid_t head = SRID_NIL;
  for (uint32_t k = 0; k < NN; k++) {
    e_smrec_t val = store_add_object(test->s, ctuple,
      k+1, head, k ? crosslink[rand()%k] : NULL); bt_chkerr(val.err);
    head = val.smrec->id;
    crosslink[k] = val.smrec;
    bt_assert_int_equal(head - ctuple->id, k + 1);
  }

  {
    uint8_t ccmp[36], * c = ccmp;
    c = sdisk_u32_write(c, SRID_NIL);
    c = sdisk_u16_write(c, 3);

    c = sdisk_u16_write(c, SKIND_UINT32);
    c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_rid_write(c, SRID_NIL);

    c = sdisk_u16_write(c, SKIND_ODREF);
    c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_rid_write(c, SRID_NIL);

    c = sdisk_u16_write(c, SKIND_OBJECT);
    c = sdisk_rid_write(c, SRID_NIL);
    c = sdisk_rid_write(c, SRID_NIL);

    e_sdrec_t er = spman_get(&test->s->pm, ctuple->id); bt_chkerr(er.err);
    bt_assert_int_equal(er.sdrec.size, 36);
    bt_assert(memcmp(er.sdrec.slot, ccmp, 36) == 0);
  }

  printf("[store] force discard\n"); gc_collect(&test->s->g, 1);

  struct tstruct {
    uint32_t n;
    soref_t  next;
  } * p;

  e_smrec_t er = store_get_object(test->s, head); bt_chkerr(er.err);
  p = er.smrec->ptr;

  for (uint32_t k = 0; k < NN; k++) {
    bt_assert_int_equal(p->n, NN - k);
    if (k != (NN-1)) {
      bt_chkerr(store_follow_ref(test->s, er.smrec, &p->next));
      p = p->next.ref->ptr;
      bt_assert_ptr_not_equal(p, NULL);
    } else {
      bt_assert_int_equal(p->next.rid, SRID_NIL);
    }
  }

  printf("[store] going to check gc (locking record & forced discard)\n");
  gc_add_root(&test->s->g, GC_OBJ(er.smrec));
  gc_collect(&test->s->g, 1);

  e_smrec_t en = store_get_object(test->s, head); bt_chkerr(en.err);
  bt_assert_ptr_equal(er.smrec, en.smrec);
  printf("[store] unlocking record & forced discard\n");
  gc_del_root(&test->s->g, GC_OBJ(er.smrec));
  gc_collect(&test->s->g, 1);
  return BT_RESULT_OK;
}

/*************************************************************************************************/
#if 0

#include "store/dexpr.h"

BT_TEST_DEF(store, expr, object, "tests for expr")
{
  struct store_test * test = object;

  e_sclass_t e;

  e = store_add_class(test->s, NULL, 2, (scfld_t []) {
    SCFLDDEF(SKIND_UINT32, NULL),
    SCFLDDEF(SKIND_UINT32, NULL),
  });
  bt_chkerr(e.err);
  sclass_t * carg = e.sclass;

  e = store_add_class(test->s, NULL, 1, (scfld_t []) {
    SCFLDDEF(SKIND_UINT32, NULL),
  });
  bt_chkerr(e.err);
  sclass_t * cres = e.sclass;

  e_smrec_t earg = store_add_object(test->s, carg, 1, 1);
  bt_chkerr(earg.err);

  e_smrec_t eops = dexpr_create_ops(test->s, 5, DEXK_ADD, DEXK_INDEX, 0, DEXK_INDEX, 1);
  bt_chkerr(eops.err);

  e_smrec_t eres = dexpr_eval(test->s, earg.smrec, eops.smrec);
  bt_chkerr(eres.err);

  return BT_RESULT_OK;
}

#endif

/*************************************************************************************************/

BT_SUITE_TEARDOWN_DEF(store, objectref)
{
  struct store_test * test = *objectref;

  store_clear(test->s);

  free(test);

  //unlink(store_test_path);

  bt_chkerr(err_pop());

  return BT_RESULT_OK;
}

#endif /* TEST */
