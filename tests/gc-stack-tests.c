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

BT_SUITE_DEF(gc_stack, "garbage collected stack tests");

BT_TEST_DEF_PLAIN(gc_stack, plain, "plain")
{
  gc_global_t     g[1];
  mem_allocator_t a;
  gc_stack_t    * s;

  a.realloc = plain_realloc;
  a.ud = NULL;

  bt_assert_ptr_not_equal(gc_global_init(g, a), NULL);

  s = gc_mem_new_obj(g, &gc_stack_vtable, sizeof(gc_stack_t));
  bt_assert_ptr_not_equal(s, NULL);

  gc_add_root_obj(g, &s->gco);

  char buf[20] = {0};

  for (unsigned k = 0; k < 10; k++) {
    snprintf(buf, 9, "run %u", k);
    gc_hdr_t * h = &gc_mem_new_str(g, buf, sizeof(buf))->gch;
    gc_stack_push(s, h);
    gc_barrier(g, &s->gco, h);
    gc_step(g);
  }

  gc_stack_set(s, 100);
  gc_stack_set(s, 4);
  gc_step(g);
  gc_step(g);
  gc_step(g);
  gc_full_gc(g);

  gc_del_root_obj(g, &s->gco);
  for (unsigned k = 0; k < s->st; k++) {
    gc_str_t * str = (gc_str_t *) s->s[k];
    bt_log("%.*s\n", (int) str->len, str->data);
  }

  gc_full_gc(g);

  gc_global_clear(g);

  return BT_RESULT_OK;
}

#endif /* TEST */
