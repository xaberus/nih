#ifndef _GC_H
#define _GC_H

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#define GC_FLAG_WHITE0 0x01
#define GC_FLAG_WHITE1 0x02
#define GC_FLAG_WHITES (GC_FLAG_WHITE0 | GC_FLAG_WHITE1)
#define GC_FLAG_BLACK  0x04
#define GC_FLAG_COLORS (GC_FLAG_WHITES | GC_FLAG_BLACK)
#define GC_FLAG_FINAL  0x08
#define GC_FLAG_FIXED  0x10
#define GC_FLAG_SFIXED 0x20

typedef struct gc_vtable gc_vtable_t;

typedef struct gc_obj {
  gc_vtable_t   * vtable;
  uint32_t        flag;
  struct gc_obj * next;
  struct gc_obj * list;
} gc_obj_t;

typedef struct gc_str {
  uint32_t        flag;
  uint32_t        hash;
  struct gc_str * next;
  char            data[];
} gc_str_t;

typedef enum gc_state {
  GC_STATE_PAUSE = 0,
  GC_STATE_PROPAGATE,
  GC_STATE_ATOMIC,
  GC_STATE_SWEEP_STRING,
  GC_STATE_SWEEP,
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
} gc_store_t;

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
  gc_store_t      objects;
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

typedef size_t (gc_function_t)(gc_global_t * g, void * o);
struct gc_vtable {
  gc_function_t * gc_init;
  gc_function_t * gc_clear;
  gc_function_t * gc_propagate;
  gc_function_t * gc_finalize;
};

void       gc_init(gc_global_t * g, mem_allocator_t alloc);
void       gc_clear(gc_global_t * g);

size_t     gc_collect(gc_global_t * g, bool full);

gc_str_t * gc_new_str(gc_global_t * g, const char * str, uint32_t len);
void *     gc_new_obj(gc_global_t * g, gc_vtable_t * vtable, uint32_t size);

void       gc_add_root(gc_global_t * g, gc_obj_t * o);
void       gc_del_root(gc_global_t * g, gc_obj_t * o);

void       gc_barrier_os(gc_global_t * g, gc_obj_t * o, gc_str_t * s);
void       gc_barrier_oo(gc_global_t * g, gc_obj_t * o, gc_obj_t * v);
void       gc_barrier_back_o(gc_global_t * g, gc_obj_t * o);


void       gc_mark_obj_i(gc_global_t * g, gc_obj_t * o);
void       gc_mark_str_i(gc_global_t * g, gc_str_t * s);

inline static
void gc_mark_obj(gc_global_t * g, gc_obj_t * o)
{
  if (((o)->flag & GC_FLAG_WHITES)) {
    gc_mark_obj_i(g, o);
  }
}

inline static
void gc_mark_str(gc_global_t * g, gc_str_t * s)
{
  if (((s)->flag & GC_FLAG_WHITES))
    gc_mark_str_i(g, s);
}

#define gc_barrier_back(g, o, v) \
  do { \
    if (((v)->flag & GC_FLAG_WHITES) && ((o)->flag & GC_FLAG_BLACK)) { \
      gc_barrier_back_o(g, o); \
    } \
  } while (0)

#endif /* _GC_H */
