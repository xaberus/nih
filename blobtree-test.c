#ifdef TEST
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/

BT_SUITE_DEF(blob, "blob allocator tests");

int _blob_sexp_check_r(blob_t * blob, sexp_t * sx);

static inline
int _blob_sexp_wrap(blob_t * blob, sexp_t * value)
{
  int err = 0;

  if (value->ty == SEXP_VALUE && (strncmp(value->val, "nill", value->val_used) == 0)) {
    err = !(blob == NULL);
  } else if (value->ty == SEXP_LIST && value->list) {
    if (!blob) goto fail;
    err = _blob_sexp_check_r(blob, value);
  } else err = 1;

fail:
  return err;
}

static inline
int _blob_sexp_record_check(blob_t * blob, sexp_t * c)
{
  int      err = 0;
  sexp_t * key, * value;
  size_t   s;
  blob_t * b;

  if (c->ty != SEXP_LIST || !c->list) goto fail; key = c->list;
  if (key->ty != SEXP_VALUE) goto fail;
  if (strncmp(key->val, "rev", key->val_used) == 0) {
    value = key->next; if (!value) goto fail;
    err = _blob_sexp_wrap(blob->rev, value);
  } else if (strncmp(key->val, "next", key->val_used) == 0) {
    value = key->next; if (!value) goto fail;
    err = _blob_sexp_wrap(blob->next, value);
  } else if (strncmp(key->val, "size", key->val_used) == 0) {
    value = key->next; s = atol(value->val);
#if 0
    err = !(s == blob->size);
#else
    err = !(s == blob->size - sizeof(blob_t));
#endif
  } else if (strncmp(key->val, "flags", key->val_used) == 0) {
    for (value = key->next; !err && value; value = value->next) {
      if (value->ty != SEXP_VALUE) goto fail;
      if (strncmp(value->val, "free", value->val_used) == 0) {
        /* noop */
      } else if (strncmp(value->val, "pool", value->val_used) == 0) {
        /* noop */
      } else if (strncmp(value->val, "poolmem", value->val_used) == 0) {
        /* noop */
      } else if (strncmp(value->val, "first", value->val_used) == 0) {
        err = !blob->flags.first;
      } else if (strncmp(value->val, "destructed", value->val_used) == 0) {
        err = !blob->flags.destructed;
      } else if (strncmp(value->val, "indestructible", value->val_used) == 0) {
        err = !blob->flags.indestructible;
      } else goto fail;
    }
  } else if (strncmp(key->val, "name", key->val_used) == 0) {
    value = key->next; if (!value || value->ty != SEXP_VALUE) goto fail;
    err = !(strncmp(value->val, blob->name, value->val_used) == 0);
  } else if (strncmp(key->val, "child", key->val_used) == 0) {
    for (value = key->next, b = blob->child;
         !err && value;
         value = value->next, b = b ? b->next : NULL) {
      if (!b) goto fail;
      err = _blob_sexp_wrap(b, value);
    }
  } else goto fail;

  return err;
fail:
  return 1;
}

int _blob_sexp_check_r(blob_t * blob, sexp_t * sx)
{
  int      err = 0;
  sexp_t * sblob, * c;

  if (sx->ty != SEXP_LIST || !sx->list) goto fail; sblob = sx->list;
  if (sblob->ty != SEXP_VALUE) goto fail;
  if (strncmp(sblob->val, "blob", sblob->val_used) == 0) {
    if (sblob->next) {
      for (c = sblob->next; !err && c; c = c->next) {
        err = _blob_sexp_record_check(blob, c);
      }
    }
  } else {
    err = _blob_sexp_record_check(blob, sx);
  }
  return err;
fail:
  return 1;
}

int blob_sexp_check(blob_t * blob, const char spec[])
{
  int       err = 1;
  size_t    len = strlen(spec);
  sexp_t  * sx = NULL;
  pcont_t * cont = NULL;
  char      buff[len];



  memcpy(buff, spec, len);

  cont = init_continuation(buff);
  sx = iparse_sexp(buff, len, cont);
  if (sx) {
    err = _blob_sexp_check_r(blob, sx);
    destroy_sexp(sx);
  }
  destroy_continuation(cont);
  sexp_cleanup();

  return err;
}

static inline
void _blop_dump_level(unsigned int level)
{
  for (unsigned int i = 0; i < level; i++)
    bt_log("  ");
}

#if 0
const char * _blop_dumper_brak[2] = {"[", "]"};
#else
const char * _blop_dumper_brak[2] = {"", ""};
#endif
const char * _blop_dumper_angl[2] = {"~> <", ">"};

static
void _blob_dumper(blob_t * blob, unsigned int level, const char * rep, const char * brk[2])
{
  if (!brk)
    brk = _blop_dumper_brak;

  if (blob->flags.pool && !rep)
    rep = "pool";

  _blop_dump_level(level); bt_log("%s %s ", brk[0], rep ? rep : "blob");

  {
    bt_log("[%4zu]{",
        blob->flags.pool ? (blob->size - sizeof(pool_t)) : (blob->size - sizeof(blob_t)));
    if (blob->flags.first)
      bt_log("\\");
    else
      bt_log("»");
    if (blob->flags.poolmem) bt_log("m");
    if (blob->flags.free) bt_log("f");
    bt_log("} ");
  }


  bt_log("[[ %8s ]]  ", blob->name);

  if (blob->flags.first) {
    if (blob->rev)
      bt_log("parent='%s'  ", blob->rev->name);
  } else {
    if (blob->rev)
      bt_log("prev='%s'  ", blob->rev->name);
  }
  if (blob->next)
    bt_log("next='%s'  ", blob->next->name);
  if (blob->child)
    bt_log("child='%s'  ", blob->child->name);
  if (blob->flags.poolmem)
    bt_log("pool='%s'  ", blob->pool->name);

  bt_log("%s\n", brk[1]);
}

static
void _blob_dump(blob_t * blob, unsigned int level)
{
  blob_t * c;

  if (blob) {
    _blob_dumper(blob, level, NULL, NULL);

    for (c = blob->child; c; c = c->next) {
      _blob_dump(c, level + 1);
    }
  }
}

void blob_dump(blob_t * ctx)
{
  _blob_dump(ctx, 0);
}

struct blob_test {
  blob_t * null_ctx;
  blob_t * root;
};

static inline
blob_t * blob_test_alloc(blob_t * ctx, size_t size, const char * name)
{
  blob_t * blob = blob_alloc(ctx, sizeof(blob_t) + size);

  if (blob)
    blob->name = name;

  return blob;
}

static inline
blob_t * blob_test_realloc(blob_t * ctx,
    blob_t * blob,
    size_t size,
    blob_t * null_ctx,
    const char * name)
{
  blob_t * re =  blob_realloc(ctx, blob, size ? (sizeof(blob_t) + size) : 0, null_ctx);

  if (re && !blob)
    re->name = name;
  return re;
}


static inline
pool_t * pool_test_alloc(blob_t * ctx, size_t size, const char * name)
{
  pool_t * pool = pool_alloc(ctx, size);

  if (pool)
    pool->name = name;

  return pool;
}

BT_SUITE_SETUP_DEF(blob)
{
  struct blob_test * test = calloc(1, sizeof(struct blob_test));

  if (!test)
    return BT_RESULT_FAIL;

  bt_assert_ptr_not_equal(
      (test->null_ctx = blob_test_alloc(NULL, 0, "null_ctx")),
      NULL);
  bt_assert_ptr_not_equal(
      (test->root = blob_test_alloc(NULL, 0, "root")),
      NULL);

  *object = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, pre, "tests preconditions and uncommon errors")
{
  struct blob_test * test = object;
  blob_t           * blob, * ptr;

  bt_assert_ptr_not_equal(
      (blob = blob_test_alloc(test->root, 10, "n1")),
      NULL); ptr = blob;
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c1.1")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c1.2")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c1.3")),
      NULL);
  bt_assert_ptr_not_equal(
      (ptr = blob_test_alloc(ptr, 10, "n2")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c2.1")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c2.2")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c2.3")),
      NULL);
  bt_assert_ptr_not_equal(
      (ptr = blob_test_alloc(ptr, 10, "n3")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c3.1")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c3.2")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c3.3")),
      NULL);
  bt_assert_ptr_not_equal(
      (ptr = blob_test_alloc(ptr, 10, "n4")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c4.1")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c4.2")),
      NULL);
  bt_assert_ptr_not_equal(
      (blob_test_alloc(ptr, 10, "c4.3")),
      NULL);

  blob_dump(test->root);
  bt_assert_int_equal(
      blob_sexp_check(test->root,
          "\
      (blob (rev nill) (next nill) (size 0) (flags first) (name root) (child\
        (blob (rev (name root)) (next nill) (size 10) (flags first) (name n1) (child\
          (blob (rev (name n1)) (size 10) (name n2) (child\
            (blob (rev (name n2)) (size 10) (name n3) (child\
              (blob (rev (name n3)) (size 10) (name n4) (child\
              ))\
            ))\
          ))\
        ))\
      ))\
    "),
      0);

  bt_assert_int_equal(
      blob_sexp_check(test->root, "(blob (name root) (child (blob (name n1) (child blah! ))))"),
      1);
  bt_assert_int_equal(
      blob_sexp_check(test->root, "(blob (name root) (child (blAb (name n1) (child blah! ))))"),
      1);


  bt_assert_int_equal(blob_free(blob, test->null_ctx), 0);
  blob_dump(test->root);

  bt_assert_int_equal(blob_total_blobs(test->root), 1);

  bt_assert_int_equal(blob_total_size(NULL), 0);
  bt_assert_int_equal(blob_total_blobs(NULL), 0);

  bt_assert_ptr_equal(
      (blob_plain_alloc(0)),
      NULL);

  bt_assert_ptr_equal(
      (pool_blob_alloc(NULL, 0)),
      NULL);

  bt_assert_ptr_equal(
      (blob_alloc(test->root, 0)),
      NULL);


  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, empty, "tests suite conditions and overflows")
{
  struct blob_test * test = object;
  blob_t           * blob;

  bt_assert_ptr_equal(
      (blob_test_alloc(test->root, 0x7fffffff, "inpossible")),
      NULL);

  bt_assert_ptr_not_equal(
      (blob = blob_test_alloc(test->root, 123, "p1")),
      NULL);
  bt_assert_int_equal(blob->size, sizeof(blob_t) * 1 + 123);
  bt_assert_int_equal(blob_free(blob, test->null_ctx), 0);

  bt_assert_int_equal(blob_free(NULL, test->null_ctx), 1);

  blob_t c = {
    .rev = NULL,
    .flags = ((struct blob_flags) {.first = 0}),
    .pool = NULL,
  };

  bt_assert_ptr_equal(
      blob_check_context(&c),
      NULL);
  c.flags.free = 1;
  bt_assert_ptr_equal(
      blob_check_context(&c),
      NULL);
  c.flags.free = 1;
  c.flags.check = BLOB_CHECK;
  bt_assert_ptr_equal(
      blob_check_context(&c),
      NULL);

  return BT_RESULT_OK;
}

static int fail_destructor_check;

static int fail_destructor(blob_t * ptr)
{
  UNUSED_PARAM(ptr);
  fail_destructor_check++;
  return 1;
}

static const blob_vtable_t fail_vtable = {
  .destructor = fail_destructor,
};

BT_TEST_DEF(blob, failing_destructor, "what happens after a failing destructor?")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4;

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 10, "p1")),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc(p1, 10, "p2")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc(p2, 10, "p3")),
      NULL);
  bt_assert_ptr_not_equal(
      (p4 = blob_test_alloc(p3, 10, "p4")),
      NULL);

  p3->vtable = &fail_vtable;
  blob_dump(root);

  int r;

  r = blob_free(p1, null);
  bt_assert_int_equal(r, 1);

  blob_dump(root);
  blob_dump(null);
  p3->vtable = NULL;
  bt_assert_int_equal(blob_free(p3, null), 0);
  blob_dump(root);
  blob_dump(null);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, realloc, "what happens on reallocation")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2;

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 10, "p1")),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(root), sizeof(blob_t) * 2 + 10);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (rev nill) (next nill) (size 0) (flags first) (name root) (child\
        (blob (rev (name root)) (next nill) (size 10) (flags first) (name p1) (child\
          ))\
      ))\
    "),
      0);


  bt_log(">> realloc p1\n");
  bt_assert_ptr_not_equal(
      (p1 = blob_test_realloc(NULL, p1, 28, null, "impossible")),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(root), sizeof(blob_t) * 2 + 28);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (rev nill) (next nill) (size 0) (flags first) (name root) (child\
        (blob (rev (name root)) (next nill) (size 28) (flags first) (name p1) (child\
          ))\
      ))\
    "),
      0);


  bt_log(">> alloc x1\n");
  bt_assert_ptr_not_equal(
      (blob_test_alloc(p1, 7, "x1")),
      NULL);

  bt_log(">> alloc p2 on p1\n");
  bt_assert_ptr_not_equal(
      (p2 = blob_test_realloc(p1, NULL, 30, null, "p2")),
      NULL);

  bt_log(">> alloc x2 on p1\n");
  bt_assert_ptr_not_equal(
      (blob_test_alloc(p1, 1, "x2")),
      NULL);
  bt_log(">> alloc x3 on p2\n");
  bt_assert_ptr_not_equal(
      (blob_test_alloc(p2, 3, "x3")),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (rev nill) (next nill) (size 0) (flags first) (child\
        (blob (name p1) (rev (name root)) (next nill) (size 28) (flags first) (child\
          (blob (name x2) (size 1))\
          (blob (name p2) (size 30) (child\
            (blob (name x3) (size 3))\
          ))\
          (blob (size 7) (name x1))\
        ))\
      ))\
    "),
      0);


  bt_log(">> realloc p2\n");
  bt_assert_ptr_not_equal(
      (p2 = blob_test_realloc(p1, p2, 40, null, 0)),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (rev nill) (next nill) (size 0) (flags first) (child\
        (blob (name p1) (rev (name root)) (next nill) (size 28) (flags first) (child\
          (blob (name x2) (size 1))\
          (blob (name p2) (size 40) (child\
            (blob (name x3) (size 3))\
          ))\
          (blob (size 7) (name x1))\
        ))\
      ))\
    "),
      0);


  bt_assert_int_equal(blob_total_size(p2), sizeof(blob_t) * 2 + 43);
  bt_assert_int_equal(blob_total_size(root), sizeof(blob_t) * 6 + 79);
  bt_assert_int_equal(blob_total_blobs(p1), 5);

  bt_log(">> realloc p1\n");
  bt_assert_ptr_not_equal(
      (p1 = blob_test_realloc(NULL, p1, 22, null, 0)),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(p1), sizeof(blob_t) * 5 + 73);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (rev nill) (next nill) (size 0) (flags first) (child\
        (blob (name p1) (rev (name root)) (next nill) (size 22) (flags first) (child\
          (blob (name x2) (size 1))\
          (blob (name p2) (size 40) (child\
            (blob (name x3) (size 3))\
          ))\
          (blob (size 7) (name x1))\
        ))\
      ))\
    "),
      0);


  bt_log(">> realloc p2\n");
  bt_assert_ptr_not_equal(
      (blob_test_realloc(NULL, p2, 5, null, 0)),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (rev nill) (next nill) (size 0) (flags first) (child\
        (blob (name p1) (rev (name root)) (next nill) (size 22) (flags first) (child\
          (blob (name x2) (size 1))\
          (blob (name p2) (size 5) (child\
            (blob (name x3) (size 3))\
          ))\
          (blob (size 7) (name x1))\
        ))\
      ))\
    "),
      0);


  bt_log(">> realloc free p2\n");
  bt_assert_ptr_equal(
      (blob_test_realloc(NULL, p2, 0, null, 0)),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (rev nill) (next nill) (size 0) (flags first) (child\
        (blob (name p1) (rev (name root)) (next nill) (size 22) (flags first) (child\
          (blob (name x2) (size 1))\
          (blob (size 7) (name x1))\
        ))\
      ))\
    "),
      0);


  bt_assert_int_equal(blob_total_blobs(p1), 3);

  bt_log(">> realloc free p1\n");
  bt_assert_ptr_equal(
      (blob_test_realloc(NULL, p1, 0, null, 0)),
      NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(root), 1);

  return BT_RESULT_OK;
}

#if 0
BT_TEST_DEF(blob, free_parent_deny_child, "talloc free parent deny child")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2, * p3;

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 10, P1)),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc(p1, 11, P2)),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc(p2, 12, P3)),
      NULL);
  p3->vtable = &fail_vtable;

  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root); blob_dump(null);

  /* only p3 should be there */
  bt_assert_int_equal(blob_total_blobs(null), 2);
  bt_assert_int_equal(blob_total_size(null), 12);
  bt_assert_int_equal(blob_total_blobs(root), 1);
  bt_assert_int_equal(blob_total_size(root), 0);

  p3->vtable = NULL;
  bt_assert_int_equal(blob_free(p3, null), 0);

  /* second run*/
  null = NULL;
  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 10, P1)),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc(p1, 11, P2)),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc(p2, 12, P3)),
      NULL);
  p3->vtable = &fail_vtable;

  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  /* only p3 should be there */
  bt_assert_int_equal(blob_total_blobs(root), 1);
  bt_assert_int_equal(blob_total_size(root), 0);

  p3->vtable = NULL;
  bt_assert_int_equal(blob_free(p3, null), 0);

  return BT_RESULT_OK;
}
#endif

static int test_free_in_destructor_check;

static
int test_free_in_destructor(blob_t * blob)
{
  blob_free(blob->child, NULL);
  test_free_in_destructor_check++;
  return 0;
}

static const blob_vtable_t test_free_in_destructor_vtable = {
  .destructor = test_free_in_destructor,
};


BT_TEST_DEF(blob, free_in_destructor, "what happens after a free in destructor?")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;

  test_free_in_destructor_check = 0;

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 10, "p1")),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc(p1, 11, "p2")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc(p2, 12, "p3")),
      NULL);
  bt_assert_ptr_not_equal(
      (p4 = blob_test_alloc(p3, 13, "p4")),
      NULL);
  bt_assert_ptr_not_equal(
      (p5 = blob_test_alloc(p4, 14, "p5")),
      NULL);

  bt_assert_int_equal(
      blob_sexp_check(root,
          "\
      (blob (name root) (child\
        (blob (name p1) (size 10) (child\
          (blob (name p2) (size 11) (child\
            (blob (name p3) (size 12) (child\
              (blob (name p4) (size 13) (child\
                (blob (name p5) (size 14))))))))))))\
    "),
      0);

  p3->vtable = &test_free_in_destructor_vtable;
  bt_assert_int_equal(blob_free(p1, null), 0);
  bt_assert_int_equal(test_free_in_destructor_check, 1);

  return BT_RESULT_OK;
}


BT_TEST_DEF(blob, pool, "does our scapegoat (double free) 'pool' behave as expected?")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;
  pool_t           * pool = NULL;

  bt_assert_ptr_equal(
      (pool_test_alloc(root, 0x7fffffff, "imposible")),
      NULL);

  /* fail */
  bt_assert_ptr_equal(
      (pool_blob_realloc(pool, NULL, NULL, 0)),
      NULL);

  bt_assert_ptr_not_equal(
      (pool = pool_test_alloc(root, 1024, "pool")),
      NULL);

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc((blob_t *) pool, 80, "p1")),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc((blob_t *) pool, 20, "p2")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc((blob_t *) pool, 40, "p3")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_realloc((blob_t *) pool, p3, 55, null, 0)),
      NULL);

  bt_assert_ptr_not_equal(
      (p5 = blob_test_alloc(p1, 40, "p5")),
      NULL);

  blob_dump(root);

  bt_log(">> realloc p3\n");
  bt_assert_ptr_not_equal(
      (p3 = blob_test_realloc((blob_t *) pool, p3, 90, null, 0)),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_realloc((blob_t *) pool, p3, 10, null, 0)),
      NULL);
  bt_assert_ptr_equal(
      (blob_test_realloc((blob_t *) pool, p3, 0x7fffffff, null, 0)),
      NULL);
  blob_dump(root);

  bt_assert_ptr_not_equal(
      (p4 = blob_test_alloc((blob_t *) pool, 4096, "p4")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_realloc((blob_t *) pool, p3, 4096, null, "p3")),
      NULL);


  bt_assert_ptr_equal(
      (blob_test_realloc(root, (blob_t *) pool, 9999, null, "impossible")),
      NULL);
  bt_assert_ptr_equal(
      (blob_test_alloc((blob_t *) pool, 0x7fffffff, "x1")),
      NULL);

  bt_assert_bool_equal(p1->flags.poolmem, true); bt_assert_ptr_equal(p1->pool, pool);
  bt_assert_bool_equal(p2->flags.poolmem, true); bt_assert_ptr_equal(p2->pool, pool);
  bt_assert_bool_equal(p5->flags.poolmem, true); bt_assert_ptr_equal(p5->pool, pool);

  bt_assert_bool_equal(p3->flags.poolmem, false);
  bt_assert_bool_equal(p4->flags.poolmem, false);

  fail_destructor_check = 0;

  p5->vtable = &fail_vtable;
  bt_assert_int_equal(blob_free(p5, null), 2);
  bt_assert_int_equal(blob_free(p5, null), 2);
  bt_assert_int_equal(fail_destructor_check, 1);
  p5->vtable = NULL;
  bt_assert_int_equal(blob_free(p5, null), 0);
  /* double free */
  bt_assert_int_equal(blob_free(p5, null), 1);

  bt_assert_int_equal(blob_free((blob_t *) pool, null), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, list, "do our inlined list functions preform as expected?")
{
  struct blob_test * test = object;
  blob_t           * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;

  bt_assert_ptr_not_equal(
      (p1 = blob_test_alloc(root, 1, "p1")),
      NULL);
  bt_assert_ptr_not_equal(
      (p2 = blob_test_alloc(root, 1, "p2")),
      NULL);
  bt_assert_ptr_not_equal(
      (p3 = blob_test_alloc(root, 1, "p3")),
      NULL);
  bt_assert_ptr_not_equal(
      (p4 = blob_test_alloc(root, 1, "p4")),
      NULL);
  bt_assert_ptr_not_equal(
      (p5 = blob_test_alloc(root, 1, "p5")),
      NULL);


  bt_assert_ptr_equal(_blob_parent(p1), root);
  blob_dump(root);

  _blob_consume(p1); bt_assert_int_equal(blob_free(p1, null), 0);
  _blob_consume(p5); bt_assert_int_equal(blob_free(p5, null), 0);
  blob_dump(root);

  _blob_list_unlink(root, p3); bt_assert_int_equal(blob_free(p3, null), 0);
  blob_dump(root);

  _blob_list_unlink(root, p4);
  _blob_list_prepend(root, p4);
  _blob_list_unlink(root, p4);
  bt_assert_int_equal(blob_free(p4, null), 0);

  _blob_list_unlink(root, p2); bt_assert_int_equal(blob_free(p2, null), 0);
  blob_dump(root);

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(blob)
{
  struct blob_test * test = *object;

  bt_assert_int_equal(blob_total_size(test->root), sizeof(blob_t));
  bt_assert_int_equal(blob_total_blobs(test->root), 1);
  bt_assert_int_equal(blob_total_size(test->null_ctx), sizeof(blob_t));
  bt_assert_int_equal(blob_total_blobs(test->null_ctx), 1);
  bt_assert_int_equal(blob_free(test->root, test->null_ctx), 0);
  bt_assert_int_equal(blob_free(test->null_ctx, NULL), 0);

  free(*object);

  return BT_RESULT_OK;
}

#endif /* TEST */

// vim: filetype=c:expandtab:shiftwidth=2:tabstop=4:softtabstop=2:encoding=utf-8:textwidth=100
