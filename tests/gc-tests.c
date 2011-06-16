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
  gc_global_t     g[1];
  mem_allocator_t a[1];
};

struct testobj {
  gc_obj_t         gco;
  struct testobj * arr[20];
  unsigned         count, flag;
};

size_t testobj_init(gc_global_t * g, void * o)
{
  (void) g;
  // bt_log("[testobj:%p] init\n", o);
  struct testobj * t = o;
  for (unsigned k = 0; k < 20; k++)
    t->arr[k] = NULL;
  t->count = 0;
  t->flag = 1;

  return 0;
}

size_t testobj_clear(gc_global_t * g, void * o)
{
  (void) g;
  // bt_log("[testobj:%p] clear\n", o);
  struct testobj * t = o;

  t->flag = 0;
  return 0;
}

size_t testobj_finalize(gc_global_t * g, void * o)
{
  (void) g;
  // bt_log("[testobj:%p] finalize\n", o);
  struct testobj * t = o;
  for (unsigned k = 0; k < t->count; k++)
    t->arr[k]->flag++;
  return 0;
}

size_t testobj_propagate(gc_global_t * g, void * o)
{
  (void) g;
  struct testobj * t = o;
  for (unsigned k = 0; k < t->count; k++) {
    // bt_log("[testobj:%p] propagate child %p\n", o, t->arr[k]);
    gc_mark_obj(g, &t->arr[k]->gco);
  }

  return 0;
}


gc_vtable_t testobj_vtable = {
  .gc_init = testobj_init,
  .gc_finalize = testobj_finalize,
  .gc_clear = testobj_clear,
  .gc_propagate = testobj_propagate,
};

BT_SUITE_SETUP_DEF(gc, objectref)
{
  struct gc_test * test = malloc(sizeof(struct gc_test));

  test->a->realloc = plain_realloc;
  test->a->ud = NULL;

  bt_assert_ptr_not_equal(test, NULL);
  bt_assert_ptr_not_equal(gc_global_init(test->g, test->a[0]), NULL);

  *objectref = test;

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(gc, objectref)
{
  struct gc_test * test = *objectref;

  gc_global_clear(test->g);

  free(test);

  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, str, object, "string tests")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;
  gc_str_t       * s;

  bt_log("[gc:str] hash size: %zu\n", g->strmask + 1);
  s = gc_mem_new_str(g, "abc", 3);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xabc" + 1, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxabc" + 2, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxabc" + 3, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxxabc" + 4, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxxxabc" + 5, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxxxxabc" + 6, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxxxxxabc" + 7, 3), s);
  bt_assert_ptr_equal(gc_mem_new_str(g, "xxxxxxxxabc" + 8, 3), s);
  bt_assert_int_equal(g->strcount, 1);
  bt_log("[gc:str] alignment ok\n");

  {
    FILE * fp;
    char   buf[512];

    if (!(fp = fopen("tests/trie-tests.txt", "r")))
      return BT_RESULT_IGNORE;

    while (fgets(buf, 512, fp)) {
      bt_assert_ptr_not_equal(gc_mem_new_str(g, buf, strlen(buf)), NULL);
    }
    fclose(fp);
    bt_log("[gc:str] hash size: %zu (%zu strings)\n", g->strmask + 1, g->strcount);
  }

  gc_full_gc(g);
  bt_log("[gc:str] hash size: %zu (%zu strings)\n", g->strmask + 1, g->strcount);

  return BT_RESULT_OK;
}

BT_TEST_DEF(gc, simple, object, "simple tests")
{
  struct gc_test * test = object;
  gc_global_t    * g = test->g;

  bt_log("{GC test with sizeof(testobj) = %zu}\n", sizeof(struct testobj));
  bt_log("[GC] total: %zu\n", g->total);

  struct testobj * o = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
  gc_add_root_obj(g, &o->gco);
  for (unsigned k = 0; k < 4; k++) {
    gc_step(g);
    o->arr[o->count++] = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
    gc_obj_barriert(g, &o->gco, &o->arr[k]->gco);
    for (unsigned j = 0; j < 4; j++) {
      o->arr[k]->arr[o->arr[k]->count++] = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
      gc_obj_barriert(g, &o->arr[k]->gco, &o->arr[k]->arr[j]->gco);
      o->arr[k]->arr[o->arr[k]->count++] = o;
      gc_obj_barriert(g, &o->arr[k]->gco, &o->gco);
    }
  }
  gc_del_root_obj(g, &o->gco);
  bt_log("[GC] total: %zu\n", g->total);
  gc_full_gc(g);
  gc_full_gc(g);
  bt_log("[GC] total: %zu\n", g->total);

  o = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
  gc_add_root_obj(g, &o->gco);
  for (unsigned k = 0; k < 4; k++) {
    gc_full_gc(g);
    o->arr[o->count++] = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
    gc_obj_barriert(g, &o->gco, &o->arr[k]->gco);
    for (unsigned j = 0; j < 4; j++) {
      o->arr[k]->arr[o->arr[k]->count++] = gc_mem_new_obj(g, &testobj_vtable, sizeof(struct testobj));
      gc_obj_barriert(g, &o->arr[k]->gco, &o->arr[k]->arr[j]->gco);
      o->arr[k]->arr[o->arr[k]->count++] = o;
      gc_obj_barriert(g, &o->arr[k]->gco, &o->gco);
      gc_full_gc(g);
    }
  }
  gc_del_root_obj(g, &o->gco);



  bt_log("[GC] total: %zu\n", g->total);
  gc_full_gc(g);
  gc_full_gc(g);
  bt_log("[GC] total: %zu\n", g->total);
  return BT_RESULT_OK;
}

#endif /* TEST */
