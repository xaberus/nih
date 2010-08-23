
#include "blobtree.h"
#include <string.h>


#define MIN(a,b) (((a)<(b)) ? (a) : (b))
#define MAX_BLOB_SIZE 0x10000000
#define BLOB_CHECK 0xd731 /* == 0y1101011100110001 */

#define BLOB_TO_DATA(_blob) ((void *)(BLOB_HDR_SIZE + (char*)(_blob)))


static
blob_t * blob_check_context(blob_t * ctx) {
  if (!ctx)
    return NULL;
  if (ctx->flags.check != BLOB_CHECK)
    return NULL;
  if (ctx->flags.free)
    return NULL;

  return ctx;
}

pool_t * pool_alloc(blob_t * ctx, size_t size)
{
  size = ALIGN4096(size + POOL_HDR_SIZE) - BLOB_HDR_SIZE;

  pool_t * pool = (pool_t *)blob_alloc(ctx, size);
  if (!pool)
    return NULL;

  pool->flags.pool = 1;
  pool->data = (char *)pool + POOL_HDR_SIZE;
  pool->count = 1;

  return  pool;
}

static
blob_t * blob_plain_alloc(size_t size)
{
  blob_t * blob = malloc(BLOB_HDR_SIZE + size);
  if (!blob)
    return NULL;

  blob->flags = ((struct blob_flags){ .free=0, });
  blob->flags.first = 1;
  blob->flags.check = BLOB_CHECK;

  blob->size = size;
  blob->vtable = NULL;
  blob->child = NULL;
  blob->refs = NULL;
  blob->rev = NULL;
  blob->next = NULL;
  blob->rev = NULL;

  return blob;
}

blob_t * pool_blob_alloc(pool_t * pool, size_t size)
{
  size_t space_left;
  blob_t * blob;
  size_t blob_size;

  pool = (pool_t *)blob_check_context((blob_t *)pool);
  if (!pool)
    return NULL;

  space_left = ((char *)pool + POOL_HDR_SIZE + pool->size) - ((char *)pool->data);
  blob_size = ALIGN16(size);

  if (space_left < blob_size)
    return NULL;

  blob = pool->data;
  blob->size = size;

  pool->data = ((char *)blob + blob_size);

  blob->flags = ((struct blob_flags){ .free=0, });
  blob->flags.first = 1;
  blob->flags.check = BLOB_CHECK;
  blob->flags.poolmem = 1;
  blob->pool = pool;

  blob->vtable = NULL;
  blob->child = NULL;
  blob->refs = NULL;
  blob->rev = NULL;
  blob->next = NULL;
  blob->rev = NULL;

  pool->count++;

  return blob;
}


static
blob_t * pool_blob_realloc(pool_t * pool, blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx)
{
  if (!blob || (blob_t *)pool != ctx || !size)
    return NULL;

  if (!ctx)
   ctx = null_ctx;

  return pool_blob_alloc(pool, size);
}

static
blob_t * blob_pool_alloc(blob_t * ctx, size_t size)
{
  if (!ctx)
    return NULL;

  if (ctx->flags.pool)
    return pool_blob_alloc((pool_t *)ctx, size);
  else if (ctx->flags.poolmem)
    return pool_blob_alloc(ctx->pool, size);

  return NULL;
}

static inline
void _blob_list_prepend(blob_t * parent, blob_t * blob)
{
  blob_t ** list = &parent->child;
  if (!*list) {
    *list = blob;
    blob->flags.first = 1;
    blob->rev = parent;
    blob->next = NULL;
  } else {
    (*list)->flags.first = 0;
    (*list)->rev = blob;
    blob->next = *list;
    blob->flags.first = 1;
    blob->rev = parent;
    *list = blob;
  }
}

static inline
void _blob_list_unlink(blob_t * parent, blob_t * blob)
{
  blob_t ** list = &parent->child;
  if (blob == *list && blob->flags.first) {
    *list = blob->next;
    if (*list) {
      (*list)->flags.first = 1;
      (*list)->rev = parent;
    }
  } else {
      if (blob->rev)
        blob->rev->next = blob->next;
      if (blob->next)
        blob->next->rev = blob->rev;
  }
  /* cleanup */
  if (blob && (blob != *list)) {
    blob->flags.first = 1;
    blob->rev = NULL;
    blob->next = NULL;
  }
}

blob_t * blob_context_alloc(blob_t * ctx, size_t size)
{
  blob_t * blob = blob_pool_alloc(ctx, BLOB_HDR_SIZE + size);

  if (!blob) {
    blob = blob_plain_alloc(size);
    if (!blob)
      return NULL;
  }

  _blob_list_prepend(ctx, blob);

  return blob;
}

blob_t * blob_alloc(blob_t * ctx, size_t size)
{
  if (size >= MAX_BLOB_SIZE)
    return NULL;

  ctx = blob_check_context(ctx);

  if (ctx)
    return blob_context_alloc(ctx, size);
  else
    return blob_plain_alloc(size);
}

void _blob_consume(blob_t * blob)
{
  if (blob->flags.first && blob->rev) {
    _blob_list_unlink(blob->rev, blob);
  } else {
    if (blob->rev)
      blob->rev->next = blob->next;
    if (blob->next)
      blob->next->rev = blob->rev;
  }
}

static inline
blob_t * _blob_parent(blob_t * blob) {
  while (blob && !blob->flags.first)
    blob = blob->rev;
  return blob->rev;
}

inline static
int _blob_is_parent(blob_t * ctx, blob_t * parent) {
  if (!ctx)
    return 0;

  blob_t * p = blob_check_context(ctx);
  while (p) {
    if (p == parent)
      return 1;
    p = _blob_parent(p);
  }

  return 0;
}

static
blob_t * _blob_steal(blob_t * new_ctx, blob_t * blob, blob_t * null_ctx)
{
  new_ctx = blob_check_context(new_ctx);
  blob = blob_check_context(blob);
  null_ctx = blob_check_context(null_ctx);

  if (!blob)
    return NULL;

  if (!new_ctx)
    new_ctx = null_ctx;

  if (!new_ctx) {
    _blob_consume(blob);

    blob->rev = blob->next = NULL;
    return blob;
  }

  if ( blob == new_ctx || (blob->flags.first && blob->rev == new_ctx))
    return blob;

  _blob_consume(blob);
  _blob_list_prepend(new_ctx, blob);

  return blob;
}

void blob_damp(blob_t * ctx, unsigned int max);

static inline
int _blob_free(blob_t * blob, blob_t * null_ctx)
{
  if (!blob)
    return -1;

  bt_log(">>>>>>>>>>>>>>>>>>>\n");
  blob_damp(blob, 5);
  bt_log("<<<<<<<<<<<<<<<<<<<\n");

  if (blob->refs) {
    int is_child = _blob_is_parent((blob_t *)blob->refs, blob);
    _blob_steal(_blob_parent((blob_t *)blob->refs), blob, null_ctx);
    _blob_free((blob_t *)blob->refs, null_ctx);
    if (is_child)
      return _blob_free(blob, null_ctx);
    return -1;
  }

  if (blob->flags.loop)
    return 0;

  if (blob->vtable && blob->vtable->destructor && !blob->flags.destr) {
    int ret;
    blob_destructor_t * d = blob->vtable->destructor;
    if (blob->flags.dmark)
      return -1;
    blob->flags.dmark = 1;
    if ((ret = d(blob))) {
      blob->flags.dmark = 0;
      return ret;
    }
    blob->flags.destr = 1;
  }

  _blob_consume(blob);

  blob->flags.loop = 1;

  while (blob->child) {
    bt_log(">>--------------->>\n");
    blob_damp(blob, 5);
    bt_log("<<---------------<<\n");

    blob_t * child = blob->child;
    blob_t * new_parent = null_ctx;
    int ret;

    if (child->refs) {
      blob_t * p = _blob_parent((blob_t *)blob->child->refs);
      if (p)
        new_parent = p;
    }
    if ((ret = _blob_free(child, null_ctx))) {
      if (new_parent == null_ctx) {
        blob_t * p = _blob_parent(blob);
        if (p)
          new_parent = p;
      }
      _blob_steal(new_parent, child, null_ctx);
    }
  }

  blob->flags.free = 1;

  if (blob->flags.pool || blob->flags.poolmem) {
    pool_t * pool;

    if (blob->flags.pool)
      pool = (pool_t *)blob;
    else if (blob->flags.poolmem)
      pool = blob->pool;

    if (!pool)
      return -1;

    if (pool->count == 0)
      return 0;

    pool->count -= 1;

    if (pool->count == 0)
      free(pool);
  } else
    free(blob);

  return 0;
}

int blob_free(blob_t * blob, blob_t * null_ctx)
{
  blob = blob_check_context(blob);

  if (!blob)
    return -1;

  if (blob->refs)
    return -1;

  return _blob_free(blob, null_ctx);
}

static inline
void _blob_reference_prepend(blob_t * blob, blob_reference_t * ref)
{
  blob_reference_t ** list = &blob->refs;
  if (!*list) {
    *list = ref;
    ref->prev_ref = ref->next_ref = NULL;
  } else {
    (*list)->prev_ref = ref;
    ref->next_ref = *list;
    ref->prev_ref = NULL;
    *list = ref;
  }
}

static inline
void _blob_reference_unlink(blob_t * blob, blob_reference_t * ref)
{
  blob_reference_t ** list = &blob->refs;
  if (ref == *list) {
    *list = ref->next_ref;
    if (*list)
      (*list)->prev_ref = NULL;
  } else {
    if (ref->prev_ref)
      ref->prev_ref->next_ref = ref->next_ref;
    if (ref->next_ref)
      ref->next_ref->prev_ref = ref->prev_ref;
  }
  if (ref && (ref != *list))
    ref->prev_ref = ref->next_ref = NULL;
}

static
int _blob_reference_destructor(blob_t * blob)
{
  blob_reference_t * ref = (blob_reference_t *)blob;
  _blob_reference_unlink(ref->blob_ref, ref);
  return 0;
}

blob_vtable_t _blob_reference_vtable = {
  .destructor = _blob_reference_destructor,
};

static
blob_reference_t * _blob_reference(blob_t * ctx, blob_t * blob)
{
  if (!blob)
    return NULL;

  blob_reference_t * ref = (blob_reference_t *)blob_alloc(ctx, BLOB_REFERENCE_HDR_ADD);

  if (!ref)
    return NULL;

  ref->flags.reference = 1;
  ref->vtable = &_blob_reference_vtable;
  ref->blob_ref = blob;

  _blob_reference_prepend(blob, ref);

  return ref;
}

blob_t * blob_reference(blob_t * ctx, blob_t * blob) 
{
  blob_reference_t * ref = _blob_reference(ctx, blob);
  if (ref)
    return ref->blob_ref;
  return NULL;
}

static inline
int _blob_unreference(blob_t * ctx, blob_t * blob, blob_t * null_ctx)
{
  blob_reference_t * h;
  blob_t * parent;

  if (!ctx)
    ctx = null_ctx;

  for (h = blob->refs; h; h = h->next_ref) {
    parent = _blob_parent((blob_t *)h);
    if (!parent && !ctx)
      break;
    else if (parent == ctx)
      break;
  }
  if (!h)
    return -1;

  return _blob_free((blob_t *)h, null_ctx);
}

int blob_unreference(blob_t * ctx, blob_t * blob, blob_t * null_ctx)
{
  return _blob_unreference(ctx, blob, null_ctx);
}

static
int _blob_unlink(blob_t * ctx, blob_t * blob, blob_t * null_ctx)
{
  blob_t * new_parent;

  if (!blob)
    return -1;

  if (!ctx)
    ctx = null_ctx;

  if (_blob_unreference(ctx, blob, null_ctx) == 0)
    return 0;

  if (!ctx && (_blob_parent(blob) != NULL))
      return -1;
  else if (ctx != _blob_parent(blob))
    return -1;

  if (!blob->refs)
    return _blob_free(blob, null_ctx);

  new_parent = _blob_parent((blob_t *)blob->refs);

  if (_blob_unreference(new_parent, blob, null_ctx) != 0)
    return -1;

  _blob_steal(new_parent, blob, null_ctx);

  return 0;
}

int _blob_free_r(blob_t * ctx, blob_t * blob, blob_t * null_ctx);

static inline
blob_t * _blob_unreference_reparent(blob_t * old_parent, blob_reference_t * ref, blob_t * blob, blob_t * null_ctx)
{
  blob_t * new_parent = _blob_parent((blob_t *)ref);

  /* self reference? */
  if (blob == new_parent) {
    _blob_free_r(blob, (blob_t *)ref, null_ctx);
    /* this was not a real reference and blob cannot be a parent */
    return NULL;
  }

  if (!_blob_free_r(new_parent, (blob_t *)ref, null_ctx)) {
    _blob_consume(blob);
    _blob_list_prepend(new_parent, blob);
    return blob;
  }
  return NULL;
}

int _blob_free_r(blob_t * ctx, blob_t * blob, blob_t * null_ctx)
{
  blob_t * parent;

  blob = blob_check_context(blob);
  ctx = blob_check_context(ctx);
  null_ctx = blob_check_context(null_ctx);

  if (!blob)
    return -1;

  if (!ctx)
    ctx = null_ctx;

  if (blob->refs) {
    blob_reference_t * ref;
    blob_reference_t * next_ref;

    /* if ctx is parent... */
    if (_blob_parent(blob) == ctx) {
      /* ... find another "parent" != NULL and unref_reparent (may corrupt ref pointer!)*/
      for (ref = blob->refs, next_ref=ref?ref->next_ref:NULL;
            ref; ref = next_ref, next_ref=ref?ref->next_ref:NULL) {
        if ((parent = _blob_parent((blob_t *)ref))) {
          if (_blob_unreference_reparent(ctx, ref, blob, null_ctx) == blob)
            /* we are done */
            return 0;
        }
      }
    }

    /* ctx is not a parent, is ctx a "parent"*/
    for (ref = blob->refs, next_ref=ref?ref->next_ref:NULL;
          ref; ref = next_ref, next_ref=ref?ref->next_ref:NULL) {
      /* ctx is ref "parent" */
      if ((parent = _blob_parent((blob_t *)ref)) == ctx) {
        _blob_unreference(parent, blob, null_ctx);
        /* we are done */
        return 0;
      }
    }
  } else {
    parent = _blob_parent(blob);
    if (parent)
      _blob_consume(blob);
  }

  /* here ctx is neither ref parent nor a real parent - blob is an orphane */

  if (blob->flags.loop)
    return 0;

  /* TODO: XXX failing destructor */
  if (blob->vtable && blob->vtable->destructor && !blob->flags.destr) {
    int ret;
    blob_destructor_t * d = blob->vtable->destructor;
    if (blob->flags.dmark)
      return -1;
    blob->flags.dmark = 1;
    if ((ret = d(blob))) {
      blob->flags.dmark = 0;
      if (null_ctx)
        _blob_list_prepend(null_ctx, blob);
      /* XXX LEAK */
      return -1;
    }
    blob->flags.destr = 1;
  }

  int ret = 0;

  blob->flags.loop = 1;
  if (blob->child) {
    blob_t * child;
    blob_t * next_child;

    for (child = blob->child, next_child=child?child->next:NULL;
          child; child = next_child, next_child=child?child->next:NULL) {
      if ((ret |= _blob_free_r(blob, child, null_ctx))) {
        /* XXX LEAK */
        if (null_ctx)
          _blob_list_prepend(null_ctx, blob);
      }
    }
  }
  blob->flags.free = 1;

    blob->flags.free = 1;

  if (blob->flags.pool || blob->flags.poolmem) {
    pool_t * pool;

    if (blob->flags.pool)
      pool = (pool_t *)blob;
    else if (blob->flags.poolmem)
      pool = blob->pool;

    if (!pool) {
      /* XXX LEAK */
      if (null_ctx)
        _blob_list_prepend(null_ctx, blob);
      return -1;
    }

    if (pool->count == 0)
      return ret;

    pool->count -= 1;

    if (pool->count == 0)
      free(pool);
  } else
    free(blob);

  return ret;

}

static
blob_t * _blob_realloc(blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx)
{
  blob_t * new;
  bool malloced = false;

  if (size == 0) {
    _blob_unlink(ctx, blob, null_ctx);
    return NULL;
  }

  if (size >= MAX_BLOB_SIZE)
    return NULL;

  if (!blob)
    return blob_alloc(ctx, size);

  blob = blob_check_context(blob);

  if (!blob || blob->refs || blob->flags.pool)
    return NULL;

  if ((size < blob->size) && ((blob->size - size) < 1024)) {
    blob->size = size;
    return blob;
  }

  blob->flags.free = 1;

  if (blob->flags.poolmem) {
    new = pool_blob_realloc(blob->pool, ctx, blob, size, null_ctx);
    blob->pool->count--;

    if (!new) {
      new = blob_plain_alloc(size);
      malloced = true;
    }

    if (new)
      memcpy(new, blob, BLOB_HDR_SIZE + MIN(blob->size, size));
  } else {
    new = realloc(blob, size + BLOB_HDR_SIZE);
  }

  if (!new) {
    blob->flags.free = 0;
    return NULL;
  }

  new->flags.free = 0;
  if (malloced)
    new->flags.poolmem = 0;
  if (new->flags.first && new->rev)
    new->rev->child = new;
  if (new->child)
    new->child->rev = new;

  if (!new->flags.first && new->rev)
    new->rev->next = new;
  if (new->next)
    new->next->rev = new;

  new->size = size;

  return new;
}

size_t blob_total_blobs(blob_t * blob) {
  size_t total = 0;
  blob_t * c;

  blob = blob_check_context(blob);

  if (!blob)
    return 0;

  if (blob->flags.loop)
    return 0;

  blob->flags.loop = 1;

  total++;
  for (c = blob->child; c; c = c->next)
    total += blob_total_blobs(c);

  blob->flags.loop = 0;

  return total;
}

size_t blob_total_size(blob_t * blob) {
  size_t total = 0;
  blob_t * c;

  blob = blob_check_context(blob);

  if (!blob)
    return 0;

  if (blob->flags.loop)
    return 0;

  blob->flags.loop = 1;

  if (!blob->flags.reference)
    total += blob->size;
  for (c = blob->child; c; c = c->next)
    total += blob_total_size(c);

  blob->flags.loop = 0;

  return total;
}

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

enum BLOB_TEST_ID {
  ERROR = 0,
  NULL_CTX,
  ROOT,
  X1,
  X2,
  X3,
  P1,
  P2,
  P3,
  P4,
  P5,
  R1,
  R2,
  R3,
  REF,
  REF1,
  REF2,
  REF3,
  POOL,
  C1,
  TOP,
  PARENT,
  CHILD,
  CHILD_OWNER,
  REQ1,
  REQ2,
  REQ3,
};

const char * blob_test_names[] = {
  [0] = "(nil)",
  [NULL_CTX] = "null_ctx",
  [ROOT] = "root",
  [X1] = "x1",
  [X2] = "x2",
  [X3] = "x3",
  [P1] = "p1",
  [P2] = "p2",
  [P3] = "p3",
  [P4] = "p4",
  [P5] = "p5",
  [R1] = "r1",
  [R2] = "r2",
  [R3] = "r3",
  [REF] = "ref",
  [REF1] = "ref1",
  [REF2] = "ref2",
  [REF3] = "ref3",
  [POOL] = "pool",
  [C1] = "c1",
  [TOP] = "top",
  [PARENT] = "parent",
  [CHILD] = "child",
  [CHILD_OWNER] = "child_owner",
  [REQ1] = "req1",
  [REQ2] = "req2",
  [REQ3] = "req3",
};

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
void _blop_dumper(blob_t * blob, unsigned int level, const char * rep, const char *brk[2])
{
  if (!brk)
    brk = _blop_dumper_brak;

  if (blob->flags.pool && !rep)
    rep = "pool";

  _blop_dump_level(level); bt_log("%s %s ", brk[0], rep?rep:"blob");

  {
#if 0
    bt_log("[%4zu]{", blob->size);
    if (blob->flags.first)
      bt_log("\\");
    else
      bt_log("»");
    if (blob->flags.poolmem) bt_log("m");
    if (blob->flags.free) bt_log("f");
    if (blob->flags.loop) bt_log("l");
    bt_log("} ");
#else
    if (blob->flags.free) bt_log("{f} ");
    if (blob->flags.loop) bt_log("{l} ");
    if (blob->flags.reference) bt_log("{*} ");
#endif
  }


  bt_log("[[ %s ]] ", blob_test_names[blob->flags.uid]);

  /*if (blob->flags.first) {
    if (blob->rev)
      bt_log("parent='%s' ", blob_test_names[blob->rev->flags.uid]);
  } else {
    if (blob->rev)
      bt_log("prev='%s' ", blob_test_names[blob->rev->flags.uid]);
  }
  if (blob->next)
    bt_log("next='%s' ", blob_test_names[blob->next->flags.uid]);
  if (blob->child)
    bt_log("child='%s' ", blob_test_names[blob->child->flags.uid]);
  if (blob->flags.poolmem)
    bt_log("pool='%s' ", blob_test_names[blob->pool->flags.uid]);*/

  bt_log("%s\n", brk[1]);
}

static 
void _blob_reference_dumper(blob_reference_t * ref, unsigned int level)
{
  blob_t * parent = _blob_parent((blob_t *)ref);
  if (parent) {
    _blop_dump_level(level+1); bt_log(" @ [ refed by ");
    bt_log(" '%s' ", blob_test_names[parent->flags.uid]);
    bt_log(" [[ %s ]] ", blob_test_names[ref->flags.uid]);
    bt_log("]\n");
  }
}

static 
void _blob_reference_blob_dumper(blob_reference_t * ref, unsigned int level)
{
  UNUSED_PARAM(ref);
  /*_blop_dumper((blob_t *)ref, level, "ref ", NULL);*/
  _blop_dumper(ref->blob_ref, level+1, NULL, _blop_dumper_angl);
}

static
void _blob_damp(blob_t * blob, unsigned int level, unsigned int max)
{
  blob_t * c;
  blob_reference_t * ref;

  if (!blob || level > max)
    return;

  _blop_dumper(blob, level, NULL, NULL);
  for (ref = blob->refs; ref; ref = ref->next_ref) {
    _blob_reference_dumper(ref, level+1);
  }
  for (c = blob->child; c; c = c->next) {
    if (c->flags.reference)
      _blob_reference_blob_dumper((blob_reference_t *)c, level+1);
    else
      _blob_damp(c, level+1, max);
  }
}

static
void _blob_dump(blob_t * blob, unsigned int level)
{
  blob_t * c;
  /*blob_reference_t * ref;*/

  if (!blob)
    return;

  if (blob->flags.loop)
    return;

  _blop_dumper(blob, level, NULL, NULL);
  /*for (ref = blob->refs; ref; ref = ref->next_ref) {
    _blob_reference_dumper(ref, level+1);
  }*/

  blob->flags.loop = 1;
  for (c = blob->child; c; c = c->next) {
    if (c->flags.reference)
      _blob_reference_blob_dumper((blob_reference_t *)c, level+1);
    else
      _blob_dump(c, level+1);
  }
  blob->flags.loop = 0;
}

void blob_damp(blob_t * ctx, unsigned int max)
{
  _blob_damp(ctx, 0, max);
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
blob_t * blob_test_alloc(blob_t * ctx, size_t size, uint32_t uid)
{
  blob_t * blob = blob_alloc(ctx, size);
  if (blob)
    blob->flags.uid = uid;

  return blob;
}

static inline
blob_t * blob_test_realloc(blob_t * ctx, blob_t * blob, size_t size, blob_t * null_ctx, uint32_t uid)
{
  blob_t * re =  _blob_realloc(ctx, blob, size, null_ctx);
  if (re && !blob)
    re->flags.uid = uid;
  return re;
}


static inline
pool_t * pool_test_alloc(blob_t * ctx, size_t size, uint32_t uid)
{
  pool_t * pool = pool_alloc(ctx, size);
  if (pool)
    pool->flags.uid = uid;

  return pool;
}


static inline
blob_t * blob_test_reference(blob_t * ctx, blob_t * blob, uint32_t uid) 
{
  blob_reference_t * ref = _blob_reference(ctx, blob);
  if (ref) {
    ref->flags.uid = uid;
    return ref->blob_ref;
  }
  return NULL;
}

static inline
blob_t * blob_test_steal(blob_t * ctx, blob_t * blob, blob_t * null_ctx) 
{
  return _blob_steal(ctx, blob, null_ctx);
}


BT_SUITE_SETUP_DEF(blob)
{
  struct blob_test * test = calloc(1, sizeof(struct blob_test));
  if (!test)
    return BT_RESULT_FAIL;

  bt_assert_ptr_not_equal(
    (test->null_ctx = blob_test_alloc(NULL, 0, NULL_CTX)),
    NULL);
  bt_assert_ptr_not_equal(
    (test->root = blob_test_alloc(NULL, 0, ROOT)),
    NULL);

  *object = test;

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, empty, "tests suite conditions")
{
  if (!object)
    return BT_RESULT_FAIL;

  struct blob_test * test = object;
  blob_t * blob;

  bt_assert_ptr_equal(
    (blob_test_alloc(test->root, 0x7fffffff, X1)),
    NULL);

  bt_assert_ptr_not_equal(
    (blob = blob_test_alloc(test->root, 123, P1)),
    NULL);
  bt_assert_int_equal(blob->size, 123);
  bt_assert_int_equal(blob_free(blob, test->null_ctx), 0);

  bt_assert_int_equal(blob_free(NULL, test->null_ctx), -1);

  blob_t c = {
    .rev = NULL,
    .flags = ((struct blob_flags){.first = 0}),
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

BT_TEST_DEF(blob, ref1, "single reference free")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * r1, * ref;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(p1, 1, P2)),
    NULL);

  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X2)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X3)),
    NULL);

  bt_assert_ptr_not_equal(
    (r1 = blob_test_alloc(root, 1, R1)),
    NULL);
  bt_assert_ptr_not_equal(
    (ref = blob_test_reference(r1, p2, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 2);

  bt_log(">> freeing %s\n", blob_test_names[p2->flags.uid]);
  bt_assert_int_equal(_blob_unlink(r1, p2, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[p1->flags.uid]);
  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[r1->flags.uid]);
  bt_assert_int_equal(blob_free(r1, null), 0);
  blob_dump(root);

  bt_assert_ptr_equal(blob_test_reference(root, NULL, REF), NULL);

  bt_assert_int_equal(blob_total_blobs(root), 1);
  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, ref2, "double reference free")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * r1, * ref;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X2)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X3)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(p1, 1, P2)),
    NULL);
  
  bt_assert_ptr_not_equal(
    (r1 = blob_test_alloc(root, 1, R1)),
    NULL);
  bt_assert_ptr_not_equal(
    (ref = blob_test_reference(r1, p2, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 2);

  bt_log(">> freeing %s\n", blob_test_names[ref->flags.uid]);
  bt_assert_int_equal(_blob_unlink(r1, ref, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[p2->flags.uid]);
  bt_assert_int_equal(blob_free(p2, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 4);
  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[p1->flags.uid]);
  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[r1->flags.uid]);
  bt_assert_int_equal(blob_free(r1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, ref3, "parent reference free")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * r1, * ref;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(root, 1, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (r1 = blob_test_alloc(p1, 1, R1)),
    NULL);
  bt_assert_ptr_not_equal(
    (ref = blob_test_reference(p2, r1, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 2);
  bt_assert_int_equal(blob_total_blobs(p2), 2);
  bt_assert_int_equal(blob_total_blobs(r1), 1);


  bt_log(">> freeing %s\n", blob_test_names[p1->flags.uid]);
  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p2), 2);
  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[p2->flags.uid]);
  bt_assert_int_equal(blob_free(p2, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, ref4, "referrer reference free")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * r1, * ref;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X2)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X3)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(p1, 1, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (r1 = blob_test_alloc(root, 1, R1)),
    NULL);
  bt_assert_ptr_not_equal(
    (ref = blob_test_reference(r1, p2, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 2);

  bt_log(">> freeing %s\n", blob_test_names[r1->flags.uid]);
  bt_assert_int_equal(blob_free(r1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 5);
  bt_assert_int_equal(blob_total_blobs(p2), 1);

  bt_log(">> freeing %s\n", blob_test_names[p2->flags.uid]);
  bt_assert_int_equal(blob_free(p2, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 4);

  bt_log(">> freeing %s\n", blob_test_names[p1->flags.uid]);
  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, unlink1, "unlink")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * r1, * ref;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X2)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 1, X3)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(p1, 1, P2)),
    NULL);

  bt_assert_ptr_not_equal(
    (r1 = blob_test_alloc(p1, 1, R1)),
    NULL);
  bt_assert_ptr_not_equal(
    (ref = blob_test_reference(r1, p2, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 7);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 2);

  bt_log(">> unreferencing %s\n", blob_test_names[r1->flags.uid]);
  bt_assert_int_equal(_blob_unlink(r1, p2, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_blobs(p1), 6);
  bt_assert_int_equal(blob_total_blobs(p2), 1);
  bt_assert_int_equal(blob_total_blobs(r1), 1);

  bt_log(">> freeing %s\n", blob_test_names[p1->flags.uid]);
  bt_assert_int_equal(blob_free(p1, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

static int fail_destructor(blob_t * ptr)
{
  UNUSED_PARAM(ptr);
  return -1;
}

static const blob_vtable_t fail_vtable = {
  .destructor = fail_destructor,
};


BT_TEST_DEF(blob, realloc, "realloc")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 10, P1)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(root), 10);

  bt_assert_ptr_not_equal(
    (p1 = blob_test_realloc(NULL, p1, 20, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(root), 20);

  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 0, X1)),
    NULL);

  bt_assert_ptr_not_equal(
    (p2 = blob_test_realloc(p1, NULL, 30, null, P2)),
    NULL);

  bt_assert_ptr_not_equal(
    (blob_test_alloc(p1, 0, X2)),
    NULL);
  blob_dump(root);

  bt_assert_ptr_not_equal(
    (p2 = blob_test_realloc(p1, p2, 40, null, 0)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_total_size(p2), 40);
  bt_assert_int_equal(blob_total_size(root), 60);
  bt_assert_int_equal(blob_total_blobs(p1), 4);

  bt_assert_ptr_not_equal(
    (p1 = blob_test_realloc(NULL, p1, 20, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_size(p1), 60);

  bt_assert_ptr_not_equal(
    (blob_reference(null, p2)),
    NULL);
  /* must fail */
  bt_assert_ptr_equal(
    (blob_test_realloc(NULL, p2, 5, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(p1), 4);

  bt_assert_ptr_equal(
    (blob_test_realloc(NULL, p2, 0, null, 0)),
    NULL);
  bt_assert_ptr_equal(
    (blob_test_realloc(NULL, p2, 0, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(p1), 4);
  bt_assert_ptr_equal(
    (blob_test_realloc(p1, p2, 0, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(p1), 3);
  
  bt_assert_ptr_equal(
    (blob_test_realloc(NULL, p1, 0, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(root), 4);
  bt_assert_ptr_equal(
    (blob_test_realloc(root, p1, 0, null, 0)),
    NULL);
  blob_dump(root);
  bt_assert_int_equal(blob_total_blobs(root), 1);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, steal, "steal")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 10, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(root, 20, P2)),
    NULL);
  bt_assert_int_equal(blob_total_size(p1), 10);
  bt_assert_int_equal(blob_total_size(root), 30);

  bt_assert_ptr_equal(
    (blob_test_steal(p1, NULL, null)),
    NULL);

  bt_assert_ptr_equal(
    (blob_test_steal(p1, p1, null)),
    p1);
  bt_assert_int_equal(blob_total_blobs(root), 3);
  bt_assert_int_equal(blob_total_size(root), 30);

  bt_assert_ptr_not_equal(
    (blob_test_steal(NULL, p1, null)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_steal(NULL, p2, null)),
    NULL);
  bt_assert_int_equal(blob_total_blobs(root), 1);
  bt_assert_int_equal(blob_total_size(root), 0);

  bt_assert_int_equal(blob_free(p1, null), 0);
  bt_assert_ptr_not_equal(
    (blob_test_steal(root, p2, null)),
    NULL);
  bt_assert_int_equal(blob_total_blobs(root), 2);
  bt_assert_int_equal(blob_total_size(root), 20);
  bt_assert_int_equal(blob_free(p2, null), 0);

  bt_assert_int_equal(blob_total_blobs(root), 1);
  bt_assert_int_equal(blob_total_size(root), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, unref_reparent, "unreference after parent freed")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * c1;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(root, 1, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (c1 = blob_test_alloc(p1, 1, C1)),
    NULL);

  bt_assert_ptr_not_equal(
    (blob_test_reference(p2, c1, REF)),
    NULL);
  blob_dump(root);

  bt_assert_ptr_equal(_blob_parent(c1), p1);

  bt_assert_int_equal(blob_free(p1, null), 0);

  blob_dump(root);

  bt_assert_ptr_equal(_blob_parent(c1), p2);

  bt_assert_int_equal(_blob_unlink(p2, c1, null), 0);

  bt_assert_int_equal(blob_total_size(root), 1);

  bt_assert_int_equal(blob_free(p2, null), 0);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, lifeless, "blob_unlink loop")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * top, * parent, * child, * child_owner;

  bt_assert_ptr_not_equal(
    (top = blob_test_alloc(root, 0, TOP)),
    NULL);
  bt_assert_ptr_not_equal(
    (child_owner = blob_test_alloc(root, 0, CHILD_OWNER)),
    NULL);


  bt_assert_ptr_not_equal(
    (parent = blob_test_alloc(top, 100, PARENT)),
    NULL);
  bt_assert_ptr_not_equal(
    (child = blob_test_alloc(top, 100, CHILD)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_reference(child, parent)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_reference(child_owner, child)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(_blob_unlink(top, parent, null), 0);
  bt_assert_int_equal(_blob_unlink(top, child, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_free(child_owner, null), 0);
  bt_assert_int_equal(blob_free(top, null), 0);

  return BT_RESULT_OK;
}

static int loop_destructor_count;

static int test_loop_destructor(blob_t * blob)
{
  UNUSED_PARAM(blob);
  loop_destructor_count++;
  bt_log("<< in test_loop_destructor()\n");
  return 0;
}

static const blob_vtable_t loop_vtable = {
  .destructor = test_loop_destructor,
};

BT_TEST_DEF(blob, loop, "loop destruction")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * top, * parent;
  struct req1 {
    blob_t super;
    blob_t *req2, *req3;
  } *req1;

  loop_destructor_count = 0;


  bt_assert_ptr_not_equal(
    (top = blob_test_alloc(root, 0, TOP)),
    NULL);

  bt_assert_ptr_not_equal(
    (parent = blob_test_alloc(top, 100, PARENT)),
    NULL);

  bt_assert_ptr_not_equal(
    (req1 = (struct req1 *)blob_test_alloc(parent, sizeof(struct req1)-BLOB_HDR_SIZE, REQ1)),
    NULL);
  bt_assert_ptr_not_equal(
    (req1->req2 = blob_test_alloc((blob_t *)req1, 100, REQ2)),
    NULL);
  req1->req2->vtable = &loop_vtable;
  bt_assert_ptr_not_equal(
    (req1->req3 = blob_test_alloc((blob_t *)req1, 100, REQ3)),
    NULL);

  bt_assert_ptr_not_equal(
    (blob_test_reference(req1->req3, (blob_t *)req1, REF)),
    NULL);
  blob_dump(root);

  bt_assert_int_equal(blob_free(parent, null), 0);
  blob_dump(root);

  bt_assert_int_equal(blob_free(top, null), 0);
  blob_dump(root);

  bt_assert_int_equal(loop_destructor_count, 1);

  return BT_RESULT_OK;
}

BT_TEST_DEF(blob, free_parent_deny_child, "talloc free parent deny child")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * p3;

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

static int test_free_in_destructor_check;

static
int test_free_in_destructor(blob_t * blob)
{
  //blob_free(*(void **)BLOB_TO_DATA(blob), NULL);
  test_free_in_destructor_check++;
  return 0;
}

static const blob_vtable_t test_free_in_destructor_vtable = {
  .destructor = test_free_in_destructor,
};

BT_TEST_DEF(blob, free_in_destructor, "free in destructor")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;

  test_free_in_destructor_check = 0;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 10, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(p1, 10, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (p3 = blob_test_alloc(p2, 10, P3)),
    NULL);
  bt_assert_ptr_not_equal(
    (p4 = blob_test_alloc(p3, 10, P4)),
    NULL);
  bt_assert_ptr_not_equal(
    (p5 = blob_test_alloc(p4, 10, P5)),
    NULL);

  *(void **)BLOB_TO_DATA(p5) = p3;

  bt_assert_ptr_not_equal(
    (blob_test_reference(root, p3, REF1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_reference(p3, p3, REF2)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_reference(p5, p3, REF3)),
    NULL);

  p5->vtable = &test_free_in_destructor_vtable;

  blob_dump(root);
  bt_assert_int_equal(_blob_free_r(root, p1, null), 0);
  blob_dump(root);
  
  bt_assert_int_equal(test_free_in_destructor_check, 1);

  bt_log("###### FIN #####\n");

  return BT_RESULT_OK;
}


BT_TEST_DEF(blob, pool, "pool")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;
  pool_t * pool = NULL;

  bt_assert_ptr_equal(
    (pool_test_alloc(root, 0x7fffffff, POOL)),
    NULL);

  /* fail */
  bt_assert_ptr_equal(
    (pool_blob_realloc(pool, NULL, NULL, 0, null)),
    NULL);

  bt_assert_ptr_not_equal(
    (pool = pool_test_alloc(root, 1024, POOL)),
    NULL);

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc((blob_t *)pool, 80, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc((blob_t *)pool, 20, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (p3 = blob_test_alloc((blob_t *)pool, 40, P3)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_test_realloc((blob_t *)pool, p3, 55, null, 0)),
    NULL);

  bt_assert_ptr_not_equal(
    (p5 = blob_test_alloc(p1, 40, P5)),
    NULL);

  blob_dump(root);

  bt_log(">> realloc %s\n", blob_test_names[p3->flags.uid]);
  bt_assert_ptr_not_equal(
    (p3 = blob_test_realloc((blob_t *)pool, p3, 90, null, 0)),
    NULL);
  bt_assert_ptr_not_equal(
    (p3 = blob_test_realloc((blob_t *)pool, p3, 10, null, 0)),
    NULL);
  bt_assert_ptr_equal(
    (blob_test_realloc((blob_t *)pool, p3, 0x7fffffff, null, 0)),
    NULL);
  blob_dump(root);

  bt_assert_ptr_not_equal(
    (p4 = blob_test_alloc((blob_t *)pool, 4096, P4)),
    NULL);

  bt_assert_ptr_equal(
    (blob_test_alloc((blob_t *)pool, 0x7fffffff, X1)),
    NULL);

  bt_assert_bool_equal(p1->flags.poolmem, true); bt_assert_ptr_equal(p1->pool, pool);
  bt_assert_bool_equal(p2->flags.poolmem, true); bt_assert_ptr_equal(p2->pool, pool);
  bt_assert_bool_equal(p3->flags.poolmem, true); bt_assert_ptr_equal(p3->pool, pool);
  bt_assert_bool_equal(p5->flags.poolmem, true); bt_assert_ptr_equal(p5->pool, pool);

  bt_assert_bool_equal(p4->flags.poolmem, false);

  bt_assert_int_equal(blob_free((blob_t *)pool, null), 0);

  return BT_RESULT_OK;
}


BT_TEST_DEF(blob, list, "checks inline list functions")
{
  struct blob_test * test = object;
  blob_t * null = test->null_ctx, * root = test->root, * p1, * p2, * p3, * p4, * p5;

  bt_assert_ptr_not_equal(
    (p1 = blob_test_alloc(root, 1, P1)),
    NULL);
  bt_assert_ptr_not_equal(
    (p2 = blob_test_alloc(root, 1, P2)),
    NULL);
  bt_assert_ptr_not_equal(
    (p3 = blob_test_alloc(root, 1, P3)),
    NULL);
  bt_assert_ptr_not_equal(
    (p4 = blob_test_alloc(root, 1, P4)),
    NULL);
  bt_assert_ptr_not_equal(
    (p5 = blob_test_alloc(root, 1, P5)),
    NULL);


  bt_assert_ptr_not_equal(
    (blob_reference(p2, p1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_reference(p3, p1)),
    NULL);
  bt_assert_ptr_not_equal(
    (blob_reference(p4, p1)),
    NULL);
  bt_assert_int_equal(
    (blob_unreference(p3, p1, null)),
    0);
  bt_assert_int_equal(
    (blob_unreference(p4, p1, null)),
    0);
  bt_assert_int_equal(
    (blob_unreference(p2, p1, null)),
    0);

  
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

  //_blob_list_unlink(root, p2); bt_assert_int_equal(blob_free(p2, null), 0);
  blob_dump(root);

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(blob)
{
  struct blob_test * test = *object;

  bt_assert_int_equal(blob_total_size(test->root), 0);
  bt_assert_int_equal(blob_total_size(test->null_ctx), 0);
  bt_assert_int_equal(_blob_free_r(NULL, test->root, test->null_ctx), 0);
  bt_assert_int_equal(_blob_free_r(NULL, test->null_ctx, NULL), 0);

  free(*object);

  return BT_RESULT_OK;
}

#endif /* TEST */

// vim: filetype=c:expandtab:shiftwidth=2:tabstop=4:softtabstop=2:encoding=utf-8:textwidth=100
