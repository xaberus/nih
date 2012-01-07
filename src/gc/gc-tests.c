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

BT_SUITE_DEF(gc, "garbage collection tests");

struct gc_test {
  gc_global_t g[1];
};

typedef struct testobj {
  gc_obj_t   gco;
  gc_hdr_t * arr[20];
  unsigned   count, flag;
} testobj_t;

static
size_t testobj_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  (void) g;
  (void) argc;
  (void) ap;
  // bt_log("[testobj:%p] init\n", o);
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < 20; k++)
    t->arr[k] = NULL;
  t->count = 0;
  t->flag = 1;

  return 0;
}

static
size_t testobj_clear(gc_global_t * g, gc_hdr_t * o)
{
  (void) g;
  // bt_log("[testobj:%p] clear\n", o);
  testobj_t * t = (testobj_t *) o;

  t->flag = 0;
  return 0;
}

static
size_t testobj_propagate(gc_global_t * g, gc_obj_t * o)
{
  (void) g;
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < t->count; k++) {
    // bt_log("[testobj:%p] propagate child %p\n", o, t->arr[k]);
    gc_mark_obj(g, (gc_obj_t *) t->arr[k]);
  }

  return 1 + t->count;
}


static
gc_vtable_t testobj_vtable = {
  .name = "testobj_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = testobj_init,
  .gc_clear = testobj_clear,
  .gc_propagate = testobj_propagate,
};

BT_SUITE_SETUP_DEF(gc, objectref)
{
  struct gc_test * test = malloc(sizeof(struct gc_test));

  bt_assert_ptr_not_equal(test, NULL);

  gc_init(test->g);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(gc, objectref)
{
  struct gc_test * test = *objectref;

  gc_clear(test->g);

  free(test);

  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, str, object, "string tests")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  gc_str_t       * s;

  bt_log("[gc:str] hash size: %u\n", g->strings.mask + 1);
  s = gc_new_str(g, 3, "abc");
  bt_assert_ptr_equal(gc_new_str(g, 3, "xabc" + 1), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxabc" + 2), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxabc" + 3), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxxabc" + 4), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxxxabc" + 5), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxxxxabc" + 6), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxxxxxabc" + 7), s);
  bt_assert_ptr_equal(gc_new_str(g, 3, "xxxxxxxxabc" + 8), s);
  bt_assert_int_equal(g->strings.count, 1);
  bt_log("[gc:str] alignment ok\n");

  {
    FILE * fp;
    char   buf[512];

    if (!(fp = fopen(BROOT "/src/trie/trie-tests.txt", "r"))) {
      bt_log("could not open test file for reading!\n");
      return BT_RESULT_IGNORE;
    }

    while (fgets(buf, 512, fp)) {
      bt_assert_ptr_not_equal(gc_new_str(g, strlen(buf), buf), NULL);
    }
    fclose(fp);
    bt_log("[gc:str] hash size: %u (%zu strings)\n", g->strings.mask + 1, g->strings.count);
  }

  gc_collect(g, 1);
  bt_log("[gc:str] hash size: %u (%zu strings)\n", g->strings.mask + 1, g->strings.count);

  return BT_RESULT_OK;
}

#define obj_barrier(g, o, v) gc_barrier_back(g, &o->gco, GC_HDR(v))

BT_TEST_DEF(gc, pressure, object, "tests behaviour unter collect pressure")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * o;

  bt_log("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  bt_log("[GC] total: %zu\n", g->total);

  o = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
  gc_add_root(g, &o->gco);

  const unsigned   N = 10;

  for (unsigned j = 0; j < N; j++) {
    testobj_t * lj = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
    o->arr[o->count++] = (gc_hdr_t *) lj;
    obj_barrier(g, o, lj);
    gc_collect(g, 0);
    for (unsigned k = 0; k < N; k++) {
      testobj_t * lk = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      obj_barrier(g, lj, lk);
      gc_collect(g, 0);
      for (unsigned l = 0; l < N; l++) {
        testobj_t * ll = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
        lk->arr[lk->count++] = (gc_hdr_t *) ll;
        obj_barrier(g, lk, ll);
        gc_collect(g, 0);
        for (unsigned m = 0; m < N; m++) {
          testobj_t * lm = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
          ll->arr[ll->count++] = (gc_hdr_t *) lm;
          obj_barrier(g, ll, lm);
          gc_collect(g, 0);
        }
      }
    }
  }

  gc_del_root(g, &o->gco);
  bt_log("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_assert_int_equal(g->total, 0);
  bt_log("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, cycles, object, "cycles should not matter at all")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * a;
  testobj_t      * b;
  testobj_t      * c;

  a = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
  b = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
  c = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);

  a->arr[a->count++] = (gc_hdr_t *) b;
  a->arr[a->count++] = (gc_hdr_t *) c;
  b->arr[b->count++] = (gc_hdr_t *) c;
  b->arr[b->count++] = (gc_hdr_t *) a;
  c->arr[c->count++] = (gc_hdr_t *) a;
  c->arr[c->count++] = (gc_hdr_t *) b;

  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_assert_int_equal(g->total, 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, misuse, object, "tests behaviour under collection misuse")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * o;

  bt_log("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  bt_log("[GC] total: %zu\n", g->total);

  o = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
  gc_add_root(g, &o->gco);
  for (unsigned j = 0; j < 20; j++) {
    gc_collect(g, 1);
    testobj_t * lj = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
    o->arr[o->count++] = (gc_hdr_t *) lj;
    obj_barrier(g, o, lj);
    gc_collect(g, 1);
    for (unsigned k = 0; k < 20; k++) {
      gc_collect(g, 1);
      testobj_t * lk = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0);
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      obj_barrier(g, lj, lk);
      gc_collect(g, 1);
    }
  }

  gc_del_root(g, &o->gco);

  bt_log("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_log("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

static
size_t testobj_genpropagate(gc_global_t * g, gc_obj_t * o)
{
  (void) g;
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < t->count; k++) {
    // bt_log("[testobj:%p] propagate child %p\n", o, t->arr[k]);
    gc_mark(g, t->arr[k]);
  }

  return 1 + t->count;
}


static
gc_vtable_t testobj_genvtable = {
  .name = "testobj_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = testobj_init,
  .gc_clear = testobj_clear,
  .gc_propagate = testobj_genpropagate,
};

BT_TEST_DEF(gc, general, object, "tests generalized behaviour")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * o;

  bt_log("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  bt_log("[GC] total: %zu\n", g->total);

  o = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0);
  gc_add_root(g, &o->gco);

  const unsigned   N = 10;

  for (unsigned j = 0; j < N; j++) {
    testobj_t * lj = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0);
    o->arr[o->count++] = (gc_hdr_t *) lj;
    o->arr[o->count++] = (gc_hdr_t *) gc_new_strf(g, "str after %x", (void *) lj);
    obj_barrier(g, o, lj);
    gc_collect(g, 0);
    for (unsigned k = 0; k < N; k++) {
      testobj_t * lk = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0);
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      lj->arr[lj->count++] = (gc_hdr_t *) gc_new_strf(g, "str after %x", (void *) lk);
      obj_barrier(g, lj, lk);
      gc_collect(g, 0);
      for (unsigned l = 0; l < N; l++) {
        testobj_t * ll = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0);
        lk->arr[lk->count++] = (gc_hdr_t *) ll;
        lk->arr[lk->count++] = (gc_hdr_t *) gc_new_strf(g, "str after %x", (void *) ll);
        obj_barrier(g, lk, ll);
        gc_collect(g, 0);
        for (unsigned m = 0; m < N; m++) {
          testobj_t * lm = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0);
          ll->arr[ll->count++] = (gc_hdr_t *) lm;
          ll->arr[ll->count++] = (gc_hdr_t *) gc_new_strf(g, "str after %x", (void *) lm);
          obj_barrier(g, ll, lm);
          gc_collect(g, 0);
        }
      }
    }
  }

  gc_del_root(g, &o->gco);
  bt_log("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_assert_int_equal(g->total, 0);
  bt_log("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

#endif /* TEST */
