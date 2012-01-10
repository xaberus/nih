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

typedef struct testobj {
  gc_obj_t         gco;
  struct testobj * objv[20];
  unsigned         ocount;
  gc_str_t       * strv[20];
  unsigned         scount;
} testobj_t;

static
err_r * testobj_init(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap)
{
  (void) g;
  (void) argc;
  (void) ap;
  // bt_log("[testobj:%p] init\n", o);
  testobj_t * t = (testobj_t *) o;

  for (unsigned k = 0; k < sizeof(t->objv)/sizeof(t->objv[0]); k++) {
    t->objv[k] = NULL;
  }
  t->ocount = 0;

  for (unsigned k = 0; k < sizeof(t->strv)/sizeof(t->strv[0]); k++) {
    t->objv[k] = NULL;
  }
  t->scount = 0;

  return NULL;
}

static
size_t testobj_clear(gc_global_t * g, gc_hdr_t * o)
{
  (void) g;
  // bt_log("[testobj:%p] clear\n", o);
  testobj_t * t = (testobj_t *) o;

  for (unsigned k = 0; k < sizeof(t->objv)/sizeof(t->objv[0]); k++) {
    t->objv[k] = NULL;
  }
  t->ocount = 0;

  for (unsigned k = 0; k < sizeof(t->strv)/sizeof(t->strv[0]); k++) {
    t->objv[k] = NULL;
  }
  t->scount = 0;

  return 0;
}

static
size_t testobj_propagate(gc_global_t * g, gc_obj_t * o)
{
  (void) g;
  testobj_t * t = (testobj_t *) o;
  for (unsigned k = 0; k < t->ocount; k++) {
    // bt_log("[testobj:%p] propagate child %p\n", o, t->arr[k]);
    gc_mark_obj(g, &t->objv[k]->gco);
  }
  for (unsigned k = 0; k < t->scount; k++) {
    // bt_log("[testobj:%p] propagate child %p\n", o, t->arr[k]);
    gc_mark_str(g, t->strv[k]);
  }

  return 1 + t->ocount + t->scount;
}


static
gc_vtable_t testobj_vtable = {
  .name = "testobj_t",
  .flag = GC_VT_FLAG_OBJ,
  .gc_init = testobj_init,
  .gc_clear = testobj_clear,
  .gc_propagate = testobj_propagate,
};

#define TESTOBJ(_o) \
  (testobj_t *) \
    (_o ? ((*((__typeof(testobj_vtable) **) _o) == &testobj_vtable) ? _o : (NULL)) \
       : (NULL))

BT_SUITE_DEF(gc_stack, "tests garbage collected stack (gc:vector, gc:barrier, gc:str, etc.)");

BT_TEST_DEF_PLAIN(gc_stack, plain, "plain")
{
  gc_global_t     g[1];
  gc_stack_t    * st;
  testobj_t     * tk, * tj, * tm;

  gc_init(g);

  e_void_t e = gc_new(g, &gc_stack_vtable, sizeof(gc_stack_t), 0);
  bt_chkerr(e.err); st = e.value; bt_assert_ptr_not_equal(st, NULL);
  gc_add_root(g, &st->gco);

  char buf[20] = {0};

  unsigned run = 0;

  for (unsigned k = 0; k < 10; k++) {
    e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tk = e.value;

    for (unsigned l = 0; l < 2; l++) {
      snprintf(buf, 9, "run %u", run++);
      e_gc_str_t e = gc_new_str(g, sizeof(buf), buf); bt_chkerr(e.err);
      tk->strv[tk->scount++] = e.gc_str;
    }

    for (unsigned j = 0; j < 10; j++) {
      e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tj = e.value;
      tk->objv[tk->ocount++] = tj;

      for (unsigned l = 0; l < 2; l++) {
        snprintf(buf, 9, "run %u", run++);
        e_gc_str_t e = gc_new_str(g, sizeof(buf), buf); bt_chkerr(e.err);
        tj->strv[tj->scount++] = e.gc_str;
      }

      for (unsigned m = 0; m < 10; m++) {
        e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tm = e.value;
        tj->objv[tj->ocount++] = tm;

        for (unsigned l = 0; l < 2; l++) {
          snprintf(buf, 9, "run %u", run++);
          e_gc_str_t e = gc_new_str(g, sizeof(buf), buf); bt_chkerr(e.err);
          tm->strv[tm->scount++] = e.gc_str;
        }
      }
    }

    gc_stack_push(g, st, tk);
    gc_collect(g, 0);
  }

  gc_collect(g, 0);
  gc_collect(g, 0);
  gc_collect(g, 1);

  bt_log("[GC] total = %zu\n", g->total);

  gc_stack_set(st, 20); bt_assert((st->de - st->d) >= 20);
  gc_stack_set(st, 4); bt_assert((st->dp - st->d) == 4);
  gc_stack_set(st, 20); bt_assert((st->de - st->d) >= 20);

  testobj_t * t;

  for (gc_hdr_t ** c = st->d; c && c < st->dp; c++) {
    if ((t = TESTOBJ(*c))) {
      bt_log("%% %+4ld testobj:%p\n", (c-st->d), (void *) t);
      for (unsigned l = 0; l < t->scount; l++) {
        gc_str_t * s = t->strv[l];
        bt_log("%%%% %+4d str:%p = '%.*s'\n", l, (void *) s, gc_str_len(s), s->data);
      }
    } else {
      bt_assert(0);
    }
  }

  gc_stack_set(st, 100); bt_assert((st->de - st->d) >= 100);
  gc_stack_set(st, 4); bt_assert((st->dp - st->d) == 4);
  gc_collect(g, 0);
  gc_collect(g, 0);
  gc_collect(g, 1);

  bt_log("[GC] total = %zu\n", g->total);

  gc_stack_set(st, 0); bt_assert_ptr_equal(gc_stack_top(st), NULL);
  bt_assert_int_equal(gc_stack_size(st), 0);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err);
  bt_chkerr(gc_stack_push(g, st, e.value));
  bt_assert_int_equal(gc_stack_size(st), 1);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err);
  bt_chkerr(gc_stack_push(g, st, e.value));
  bt_assert_int_equal(gc_stack_size(st), 2);

  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err);
  bt_chkerr(gc_stack_push(g, st, e.value));
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err);
  bt_chkerr(gc_stack_push(g, st, e.value));
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err);
  bt_chkerr(gc_stack_push(g, st, e.value));
  bt_assert_int_equal(gc_stack_size(st), 5);

  gc_stack_pop(st);
  bt_assert_int_equal(gc_stack_size(st), 4);
  gc_stack_pop(st);
  gc_stack_pop(st);
  gc_stack_pop(st);
  gc_stack_pop(st);
  bt_assert_int_equal(gc_stack_size(st), 0);
  gc_stack_pop(st);
  gc_stack_pop(st);
  gc_stack_pop(st);
  gc_stack_pop(st);
  bt_assert_int_equal(gc_stack_size(st), 0);

  testobj_t * tst[6];
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[0] = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[1] = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[2] = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[3] = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[4] = e.value;
  e = gc_new(g, &testobj_vtable, sizeof(testobj_t), 0); bt_chkerr(e.err); tst[5] = e.value;

  gc_stack_push(g, st, tst[0]);
  gc_stack_push(g, st, tst[1]);
  gc_stack_push(g, st, tst[2]);
  gc_stack_push(g, st, tst[3]);
  gc_stack_push(g, st, tst[4]);
  gc_stack_push(g, st, tst[5]);

  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[5]));
  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[4]));
  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[3]));
  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[2]));
  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[1]));
  bt_assert_ptr_equal(gc_stack_pop(st), GC_HDR(tst[0]));

  gc_del_root(g, &st->gco);
  gc_collect(g, 1);

  bt_log("[GC] total = %zu\n", g->total);

  gc_clear(g);

  return BT_RESULT_OK;
}

#endif /* TEST */
