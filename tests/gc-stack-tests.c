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
#include "gc-private.h"

typedef struct testobj {
  gc_obj_t         gco;
  struct testobj * objv[20];
  unsigned         ocount;
  gc_str_t       * strv[20];
  unsigned         scount;
} testobj_t;

static
size_t testobj_init(gc_global_t * g, gc_hdr_t * o)
{
  (void) g;
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

  return 0;
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

/*size_t testobj_finalize(gc_global_t * g, void * o)
{
  (void) g;
  // bt_log("[testobj:%p] finalize\n", o);
  testobj_t * t = o;
  for (unsigned k = 0; k < t->count; k++)
    t->arr[k]->flag++;
  return 1;
}*/

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
  .gc_init = testobj_init,
  .gc_finalize = /*testobj_finalize*/ NULL,
  .gc_clear = testobj_clear,
  .gc_propagate = testobj_propagate,
};

#define TESTOBJ(_o) \
  (testobj_t *) \
    (_o ? ((*((TYPEOF(testobj_vtable) **) _o) == &testobj_vtable) ? _o : (NULL)) \
       : (NULL))

BT_SUITE_DEF(gc_stack, "tests garbage collected stack (gc:vector, gc:barrier, gc:str, etc.)");

BT_TEST_DEF_PLAIN(gc_stack, plain, "plain")
{
  gc_global_t     g[1];
  mem_allocator_t a;
  gc_stack_t    * st;

  a.realloc = plain_realloc;
  a.ud = NULL;

  gc_init(g, a);

  st = gc_new_obj(g, &gc_stack_vtable, sizeof(gc_stack_t));
  bt_assert_ptr_not_equal(st, NULL);

  gc_add_root(g, &st->gco);

  char buf[20] = {0};

  unsigned run = 0;

  for (unsigned k = 0; k < 10; k++) {
    testobj_t * tk = gc_new_obj(g, &testobj_vtable, sizeof(testobj_t));

    for (unsigned l = 0; l < 2; l++) {
      snprintf(buf, 9, "run %u", run++);
      gc_str_t * s = gc_new_str(g, buf, sizeof(buf));
      tk->strv[tk->scount++] = s;
    }

    for (unsigned j = 0; j < 10; j++) {
      testobj_t * tj = gc_new_obj(g, &testobj_vtable, sizeof(testobj_t));
      tk->objv[tk->ocount++] = tj;

      for (unsigned l = 0; l < 2; l++) {
        snprintf(buf, 9, "run %u", run++);
        gc_str_t * s = gc_new_str(g, buf, sizeof(buf));
        tj->strv[tj->scount++] = s;
      }

      for (unsigned m = 0; m < 10; m++) {
        testobj_t * tm = gc_new_obj(g, &testobj_vtable, sizeof(testobj_t));
        tj->objv[tj->ocount++] = tm;

        for (unsigned l = 0; l < 2; l++) {
          snprintf(buf, 9, "run %u", run++);
          gc_str_t * s = gc_new_str(g, buf, sizeof(buf));
          tm->strv[tm->scount++] = s;
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

  gc_stack_set(g, st, 20);
  gc_stack_set(g, st, 4);
  gc_stack_set(g, st, 20);

  testobj_t * t;

  for (unsigned k = 0; k < st->count; k++) {
    if ((t = TESTOBJ(st->data[k]))) {
      bt_log("%% %+4d testobj:%p\n", k, (void *) t);
      for (unsigned l = 0; l < t->scount; l++) {
        gc_str_t * s = t->strv[l];
        bt_log("%%%% %+4d str:%p = '%.*s'\n", l, (void *) s, gc_str_len(s), s->data);
      }
    } else {
      bt_assert(0);
    }
  }

  gc_stack_set(g, st, 100);
  gc_stack_set(g, st, 4);
  gc_collect(g, 0);
  gc_collect(g, 0);
  gc_collect(g, 1);

  bt_log("[GC] total = %zu\n", g->total);

  gc_del_root(g, &st->gco);

  gc_collect(g, 1);

  bt_log("[GC] total = %zu\n", g->total);

  gc_clear(g);

  return BT_RESULT_OK;
}

#endif /* TEST */
