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
err_r * testobj_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  (void) g;
  (void) argc;
  (void) ap;
  // printf("[testobj:%p] init\n", o);
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < 20; k++)
    t->arr[k] = NULL;
  t->count = 0;
  t->flag = 1;

  return NULL;
}

static
size_t testobj_clear(gc_global_t * g, gc_hdr_t * o)
{
  (void) g;
  // printf("[testobj:%p] clear\n", o);
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
    // printf("[testobj:%p] propagate child %p\n", o, t->arr[k]);
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

  bt_chkerr(gc_init(test->g));

  *objectref = test;

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(gc, objectref)
{
  struct gc_test * test = *objectref;

  gc_clear(test->g);

  free(test);

  bt_chkerr(err_pop());

  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, str, object, "string tests")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;

#define checknstr(_n, _str, _s) \
  do { \
    const char * __str = (_str); \
    uint32_t __n = (_n); \
    e_gc_str_t __e = gc_new_str(g, __n, __str); \
    bt_chkerr(__e.err); \
    bt_assert_ptr_equal(__e.gc_str, (_s)); \
  } while(0)

  printf("[gc:str] hash size: %u\n", g->strings.mask + 1);

  e_gc_str_t e = gc_new_str(g, 3, "abc");

  checknstr(3, "xabc" + 1, e.gc_str);
  checknstr(3, "xxabc" + 2, e.gc_str);
  checknstr(3, "xxxabc" + 3, e.gc_str);
  checknstr(3, "xxxxabc" + 4, e.gc_str);
  checknstr(3, "xxxxxabc" + 5, e.gc_str);
  checknstr(3, "xxxxxxabc" + 6, e.gc_str);
  checknstr(3, "xxxxxxxabc" + 7, e.gc_str);
  checknstr(3, "xxxxxxxxabc" + 8, e.gc_str);
#undef checknstr

  bt_assert_int_equal(g->strings.count, 1);
  printf("[gc:str] alignment ok\n");

  {
    FILE * fp;
    char   buf[512];

    if (!(fp = fopen(BROOT "/src/trie/trie-tests.txt", "r"))) {
      printf("could not open test file for reading!\n");
      return BT_RESULT_IGNORE;
    }

    while (fgets(buf, 512, fp)) {
      e_gc_str_t e = gc_new_str(g, strlen(buf), buf);
      bt_chkerr(e.err);
    }
    fclose(fp);
    printf("[gc:str] hash size: %u (%zu strings)\n", g->strings.mask + 1, g->strings.count);
  }

  gc_collect(g, 1);
  printf("[gc:str] hash size: %u (%zu strings)\n", g->strings.mask + 1, g->strings.count);

  return BT_RESULT_OK;
}

#define obj_barrier(g, o, v) gc_barrier_back(g, &o->gco, GC_HDR(v))

BT_TEST_DEF(gc, pressure, object, "tests behaviour unter collect pressure")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * o;
  e_void_t         e;

  printf("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  printf("[GC] total: %zu\n", g->total);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); o = e.value;
  bt_chkerr(gc_add_root(g, &o->gco));

  const unsigned   N = 10;

  testobj_t * lj, * lk, * ll, * lm;
  for (unsigned j = 0; j < N; j++) {
    e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lj = e.value;
    o->arr[o->count++] = (gc_hdr_t *) lj;
    obj_barrier(g, o, lj);
    gc_collect(g, 0);
    for (unsigned k = 0; k < N; k++) {
      e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lk = e.value;
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      obj_barrier(g, lj, lk);
      gc_collect(g, 0);
      for (unsigned l = 0; l < N; l++) {
        e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); ll = e.value;
        lk->arr[lk->count++] = (gc_hdr_t *) ll;
        obj_barrier(g, lk, ll);
        gc_collect(g, 0);
        for (unsigned m = 0; m < N; m++) {
          e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lm = e.value;
          ll->arr[ll->count++] = (gc_hdr_t *) lm;
          obj_barrier(g, ll, lm);
          gc_collect(g, 0);
        }
      }
    }
  }

  gc_del_root(g, &o->gco);
  printf("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_assert_int_equal(g->total, 0);
  printf("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, cycles, object, "cycles should not matter at all")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  testobj_t      * a, * b, * c;
  e_void_t       e;

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); a = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); b = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); c = e.value;

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
  testobj_t      * o, * lj, * lk;
  e_void_t         e;

  printf("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  printf("[GC] total: %zu\n", g->total);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); o = e.value;
  gc_add_root(g, &o->gco);
  for (unsigned j = 0; j < 20; j++) {
    gc_collect(g, 1);
    e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lj = e.value;
    o->arr[o->count++] = (gc_hdr_t *) lj;
    obj_barrier(g, o, lj);
    gc_collect(g, 1);
    for (unsigned k = 0; k < 20; k++) {
      gc_collect(g, 1);
      e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lk = e.value;
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      obj_barrier(g, lj, lk);
      gc_collect(g, 1);
    }
  }

  gc_del_root(g, &o->gco);

  printf("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  printf("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

static
size_t testobj_genpropagate(gc_global_t * g, gc_obj_t * o)
{
  (void) g;
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < t->count; k++) {
    // printf("[testobj:%p] propagate child %p\n", o, t->arr[k]);
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
  testobj_t      * o, * lj, * lk, * ll, * lm;
  e_void_t         e;
  e_gc_str_t       ee;

  printf("{GC test with sizeof(testobj) = %zu}\n", sizeof(testobj_t));
  printf("[GC] total: %zu\n", g->total);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); o = e.value;
  gc_add_root(g, &o->gco);

  const unsigned   N = 10;

  for (unsigned j = 0; j < N; j++) {
    e = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lj = e.value;
    o->arr[o->count++] = (gc_hdr_t *) lj;
    ee = gc_new_strf(g, "str after %x", (void *) lj); bt_chkerr(ee.err);
    o->arr[o->count++] = (gc_hdr_t *) ee.gc_str;
    obj_barrier(g, o, lj);
    gc_collect(g, 0);
    for (unsigned k = 0; k < N; k++) {
      e = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lk = e.value;
      lj->arr[lj->count++] = (gc_hdr_t *) lk;
      ee = gc_new_strf(g, "str after %x", (void *) lk); bt_chkerr(ee.err);
      lj->arr[lj->count++] = (gc_hdr_t *) ee.gc_str;
      obj_barrier(g, lj, lk);
      gc_collect(g, 0);
      for (unsigned l = 0; l < N; l++) {
        e = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0); bt_chkerr(e.err); ll = e.value;
        lk->arr[lk->count++] = (gc_hdr_t *) ll;
        ee = gc_new_strf(g, "str after %x", (void *) ll); bt_chkerr(ee.err);
        lk->arr[lk->count++] = (gc_hdr_t *) ee.gc_str;
        obj_barrier(g, lk, ll);
        gc_collect(g, 0);
        for (unsigned m = 0; m < N; m++) {
          e = gc_new(g, &testobj_genvtable, sizeof(testobj_t), 0); bt_chkerr(e.err); lm = e.value;
          ll->arr[ll->count++] = (gc_hdr_t *) lm;
          ee = gc_new_strf(g, "str after %x", (void *) lm); bt_chkerr(ee.err);
          ll->arr[ll->count++] = (gc_hdr_t *) ee.gc_str;
          obj_barrier(g, ll, lm);
          gc_collect(g, 0);
        }
      }
    }
  }

  gc_del_root(g, &o->gco);
  printf("[GC] total: %zu\n", g->total);
  gc_collect(g, 1);
  gc_collect(g, 1);
  bt_assert_int_equal(g->total, 0);
  printf("[GC] total: %zu\n", g->total);

  return BT_RESULT_OK;
}

#endif /* TEST */
