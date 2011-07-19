#ifndef _GC_H
#define _GC_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

#define GC_FLAG_WHITE0 0x01
#define GC_FLAG_WHITE1 0x02
#define GC_FLAG_WHITES (GC_FLAG_WHITE0 | GC_FLAG_WHITE1)
#define GC_FLAG_BLACK  0x04
#define GC_FLAG_COLORS (GC_FLAG_WHITES | GC_FLAG_BLACK)
#define GC_FLAG_FINAL  0x08
#define GC_FLAG_FIXED  0x10
#define GC_FLAG_SFIXED 0x20

typedef struct gc_vtable gc_vtable_t;

typedef struct gc_hdr {
  gc_vtable_t   * vtable;
  uint32_t        flag;
  struct gc_hdr * next;
} gc_hdr_t;

#define GC_HDR(_o)  ((gc_hdr_t *) (_o))
#define GC_HDRP(_p) ((gc_hdr_t **) (_p))
#define GC_SIZE_MAX ((1 << 24) - 1)

typedef struct gc_obj {
  gc_hdr_t        gch;
  struct gc_obj * list;
} gc_obj_t;

typedef struct gc_str {
  gc_hdr_t gch;
  uint32_t hash;
  char     data[];
} gc_str_t;

#define gc_str_len(s) ((uint32_t) (((GC_HDR(s)->flag & 0xffffff00) >> 8) - sizeof(gc_str_t) - 1))

typedef enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
  GC_STATE_SWEEP_HEADER,
  GC_STATE_FINALIZE,
} gc_state_t;


typedef struct {
  gc_str_t  * head;
  gc_str_t ** sweep;
} gc_bucket_t;

typedef struct {
  gc_bucket_t * data;
  size_t        dsize;
  size_t        count;
  gc_bucket_t * proc;
  size_t        psize;
} gc_buckets_t;

typedef struct {
  gc_buckets_t buckets;
  uint32_t     mask;
  size_t       count;
  uint32_t     sweep;
  uint32_t     run;
} gc_strings_t;

typedef struct {
  gc_obj_t  * head;
  gc_obj_t ** sweep;
} gc_ostore_t;

typedef struct {
  gc_hdr_t  * head;
  gc_hdr_t ** sweep;
} gc_hstore_t;

typedef struct {
  gc_obj_t * head;
} gc_collect_t;

typedef struct {
  gc_obj_t * loop;
} gc_circle_t;

typedef struct {
  gc_obj_t ** data;
  size_t      dsize;
  size_t      count;
  gc_obj_t ** proc;
  size_t      psize;
} gc_roots_t;


typedef struct {
  gc_strings_t    strings;
  gc_ostore_t     objects;
  gc_hstore_t     headers;
  gc_collect_t    grey0;
  gc_collect_t    grey1;
  gc_circle_t     final;
  gc_roots_t      roots;
  gc_state_t      state;
  uint32_t        white;
  size_t          total;
  size_t          thres;
  mem_allocator_t alloc;
} gc_global_t;

struct gc_vtable {
  uint32_t     flag;
  const char * name;

  /* gc_hdr_t */
  size_t       (* gc_init)(gc_global_t * g, gc_hdr_t * o);
  size_t       (* gc_clear)(gc_global_t * g, gc_hdr_t * o);

  /* gc_obj_t */
  size_t       (* gc_propagate)(gc_global_t * g, gc_obj_t * o);
  size_t       (* gc_finalize)(gc_global_t * g, gc_obj_t * o);
};

extern gc_vtable_t gc_blob_vtable;

enum gc_vt_flags {
  GC_VT_FLAG_HDR = 0x01,
  GC_VT_FLAG_OBJ = 0x02 | GC_VT_FLAG_HDR,
  GC_VT_FLAG_STR = 0x04 | GC_VT_FLAG_HDR,
};

void       gc_init(gc_global_t * g, mem_allocator_t alloc);
void       gc_clear(gc_global_t * g);

size_t     gc_collect(gc_global_t * g, bool full);

gc_str_t * gc_new_str(gc_global_t * g, uint32_t len, const char str[len]);
gc_str_t * gc_new_strf(gc_global_t * g, const char * fmt, ...);
void *     gc_new(gc_global_t * g, gc_vtable_t * vtable, uint32_t size);

void       gc_add_root(gc_global_t * g, gc_obj_t * o);
void       gc_del_root(gc_global_t * g, gc_obj_t * o);

void       gc_barrier_oh(gc_global_t * g, gc_obj_t * o, gc_hdr_t * h);
void       gc_barrier_oo(gc_global_t * g, gc_obj_t * o, gc_obj_t * v);
void       gc_barrier_back_o(gc_global_t * g, gc_obj_t * o);

void       gc_mark_obj_i(gc_global_t * g, gc_obj_t * o);
void       gc_mark_hdr_i(gc_global_t * g, gc_hdr_t * o);
void       gc_mark_str_i(gc_global_t * g, gc_str_t * s);

inline static
void gc_mark(gc_global_t * g, gc_hdr_t * o)
{
  if (o->flag & GC_FLAG_WHITES) {
    if ((o->vtable->flag & GC_VT_FLAG_OBJ) == GC_VT_FLAG_OBJ) {
      gc_mark_obj_i(g, (gc_obj_t *) o);
    } else if ((o->vtable->flag & GC_VT_FLAG_HDR) == GC_VT_FLAG_HDR) {
      gc_mark_hdr_i(g, o);
    } else {
      assert(0);
    }
  }
}

inline static
void gc_mark_obj(gc_global_t * g, gc_obj_t * o)
{
  if ((GC_HDR(o)->flag & GC_FLAG_WHITES)) {
    gc_mark_obj_i(g, o);
  }
}

inline static
void gc_mark_str(gc_global_t * g, gc_str_t * s)
{
  if ((GC_HDR(s)->flag & GC_FLAG_WHITES)) {
    gc_mark_hdr_i(g, GC_HDR(s));
  }
}

void   gc_mem_free(gc_global_t * g, size_t size, void * p);
void * gc_mem_realloc(gc_global_t * g, size_t osz, size_t nsz, void * p);

#define gc_mem_new(g, s) \
  gc_mem_realloc(g, 0, (s), NULL)

#define gc_barrier_back(_g, _o, _v) \
  do { \
    if ((GC_HDR(_v)->flag & GC_FLAG_WHITES) && (GC_HDR(_o)->flag & GC_FLAG_BLACK)) { \
      gc_barrier_back_o((_g), (_o)); \
    } \
  } while (0)

#define gc_barrier(_g, _o, _v) \
  do { \
    if ((GC_HDR(_v)->flag & GC_FLAG_WHITES) && (GC_HDR(_o)->flag & GC_FLAG_BLACK)) { \
      gc_barrier_oh((_g), (_o), (_v)); \
    } \
  } while (0)


#define gc_error(g, ...) ((void) g, assert(0))

#endif /* _GC_H */
