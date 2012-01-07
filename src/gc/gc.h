#ifndef _GC_H
#define _GC_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#define GC_FLAG_WHITE0 0x01
#define GC_FLAG_WHITE1 0x02
#define GC_FLAG_WHITES (GC_FLAG_WHITE0 | GC_FLAG_WHITE1)
#define GC_FLAG_BLACK  0x04
#define GC_FLAG_COLORS (GC_FLAG_WHITES | GC_FLAG_BLACK)
#define GC_FLAG_FIXED  0x10
#define GC_FLAG_SFIXED 0x20

typedef struct gc_vtable gc_vtable_t;

typedef struct gc_hdr {
  gc_vtable_t   * vtable;
  uint16_t        flag;
  uint32_t        size;
  struct gc_hdr * next;
} gc_hdr_t;

#define GC_HDR(_o)  ((gc_hdr_t *) (_o))
#define GC_HDRP(_p) ((gc_hdr_t **) (_p))
#define GC_SIZE_MAX ((1 << 24) - 1)

typedef struct gc_obj {
  gc_hdr_t        gch;
  struct gc_obj * list;
} gc_obj_t;

#define GC_OBJ(_o)  ((gc_obj_t *) (_o))

typedef struct gc_str {
  gc_hdr_t gch;
  uint32_t hash;
  char     data[];
} gc_str_t;

#define gc_str_len(s) ((uint32_t) (GC_HDR(s)->size - sizeof(gc_str_t) - 1))

typedef enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
  GC_STATE_SWEEP_HEADER,
} gc_state_t;

typedef struct {
  struct {
    struct {
      struct {
        gc_str_t  * head;
        gc_str_t ** sweep;
      }    * data, * proc;
      size_t dsize, psize;
      size_t count;
    }        buckets;
    uint32_t mask;
    size_t   count;
    uint32_t sweep;
    uint32_t run;
  } strings;
  struct {
    gc_obj_t  * head;
    gc_obj_t ** sweep;
  } objects;
  struct {
    gc_hdr_t  * head;
    gc_hdr_t ** sweep;
  } headers;
  struct {
    gc_obj_t * head;
  } grey0, grey1;
  struct {
    gc_obj_t ** data;
    size_t      dsize;
    size_t      count;
    gc_obj_t ** proc;
    size_t      psize;
  } roots;
  gc_state_t      state;
  uint32_t        white;
  size_t          total;
  size_t          thres;
} gc_global_t;

struct gc_vtable {
  uint32_t     flag;
  const char * name;
  /* gc_hdr_t */
  size_t       (* gc_init)(gc_global_t * g, gc_hdr_t * o, int argc, va_list ap);
  size_t       (* gc_clear)(gc_global_t * g, gc_hdr_t * o);
  /* gc_obj_t */
  size_t       (* gc_propagate)(gc_global_t * g, gc_obj_t * o);
};

extern gc_vtable_t gc_blob_vtable;

enum gc_vt_flags {
  GC_VT_FLAG_HDR = 0x01,
  GC_VT_FLAG_OBJ = 0x02 | GC_VT_FLAG_HDR,
  GC_VT_FLAG_STR = 0x04 | GC_VT_FLAG_HDR,
};

void       gc_init(gc_global_t * g);
void       gc_clear(gc_global_t * g);

size_t     gc_collect(gc_global_t * g, bool full);

gc_str_t * gc_new_str(gc_global_t * g, uint32_t len, const char str[len]);
gc_str_t * gc_new_strf(gc_global_t * g, const char * fmt, ...);
void *     gc_new(gc_global_t * g, gc_vtable_t * vtable, uint32_t size, int argc, ...);

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
