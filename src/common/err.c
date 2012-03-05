#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#include "common/err.h"

#define ERR_MAXSTACK 128

static pthread_key_t __err_key;
static int __err_initialized = 0;

typedef struct errlist {
  err_r * list;
  err_r * orphan;
} errlist_t;

static
void __err_clear(void * e)
{
  errlist_t * elist = e;
  for (err_r * p = elist->list, * n = p ? p->next : NULL; p; p = n, n = p ? p->next : NULL) {
    free(p);
  }
  for (err_r * p = elist->orphan, * n = p ? p->next : NULL; p; p = n, n = p ? p->next : NULL) {
    free(p);
  }
  free(elist);
}


static __attribute__((constructor))
void __err_init() {
  if (pthread_key_create(&__err_key, __err_clear)) {
    exit(-1);
  }
  errlist_t * elist = malloc(sizeof(errlist_t));
  if (!elist) {
    exit(-1);
  }
  elist->list = NULL;
  elist->orphan = NULL;
  pthread_setspecific(__err_key, elist);
  __err_initialized = 1;
}

static __attribute__((destructor))
void __err_finish() {
  errlist_t * elist = pthread_getspecific(__err_key);
  pthread_setspecific(__err_key, NULL);
  pthread_key_delete(__err_key);
  __err_initialized = 0;
  __err_clear(elist);
}

static
err_r internal_err_mem_alloc = {
  .err = ERR_MEM_ALLOC,
  .file = __FILE__,
  .line = __LINE__,
  .fun = "err_push()",
  .msg = "could not allocate error stack",
  .eno = 0,
  .next = NULL,
};

err_r * err_push(err_t err, const char * fi, int li, const char * fn, const char * m)
{
  assert(__err_initialized);
  assert(fi && fn && m);
  errlist_t * elist = pthread_getspecific(__err_key);
  assert(elist);
  err_r * r = NULL;
  if (elist->orphan) {
    r = elist->orphan;
    elist->orphan = r->next;
  } else {
    r = malloc(sizeof(err_r));
    if (!r) {
      assert(0);
      return &internal_err_mem_alloc;
    }
  }
  *r = (err_r) {
    .err = err,
    .file = fi,
    .line = li,
    .fun = fn,
    .msg = m,
    .eno = errno,
    .next = elist->list,
  };
  elist->list = r;
  errno = 0;
  return r;
}

void err_reset()
{
  assert(__err_initialized);
  errlist_t * elist = pthread_getspecific(__err_key);
  assert(elist);
  for (err_r * p = elist->list, * n = p ? p->next : NULL; p; p = n, n = p ? p->next : NULL) {
    elist->list = p->next;
    p->next = elist->orphan;
    elist->orphan = p;
  }
}

err_r * err_pop()
{
  assert(__err_initialized);
  errlist_t * elist = pthread_getspecific(__err_key);
  assert(elist);
  err_r * r = elist->list;
  if (r) {
    elist->list = r->next;
    r->next = elist->orphan;
    elist->orphan = r;
  }
  return r;
}

void err_report(int fd)
{
  assert(__err_initialized);
  errlist_t * elist = pthread_getspecific(__err_key);
  assert(elist);
  err_r * r = elist->list;
  if (r) {
    int df = dup(fd);
    if (df != -1) {
      FILE * st = fdopen(df, "w");
      if (st) {
        fprintf(st, "Error report\n");
        while(r) {
          fprintf(st, "  %s() @ %s:%d\n    %s\n", r->fun, r->file, r->line, r->msg);
          r = r->next;
        }
        fclose(st);
      } else {
        fprintf(stderr, "Error report(could not open fd)\n");
        while(r) {
          fprintf(st, "  %s() @ %s:%d\n    %s\n", r->fun, r->file, r->line, r->msg);
          r = r->next;
        }
      }
    }
  }
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

#include <bt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

BT_EXPORT();

static
err_r * test_function_5()
{
  int fd = open("/not-existing-file", O_RDWR);
  if (fd == -1) {
    return err_return(ERR_FAILURE, "could not open non existing file (duty of this test)");
  }
  return err_return(ERR_FAILURE, "not existing file found");
}

static
err_r * test_function_4()
{
  err_r * e = test_function_5();
  if (e) {
    return err_return(ERR_FAILURE, "test_function_5() failed");
  }
  return NULL;
}

static
err_r * test_function_3()
{
  err_r * e = test_function_4();
  if (e) {
    return err_return(ERR_FAILURE, "test_function_4() failed");
  }
  return NULL;
}

static
err_r * test_function_2()
{
  err_r * e = test_function_3();
  if (e) {
    return err_return(ERR_FAILURE, "test_function_3() failed");
  }
  return NULL;
}

static
err_r * test_function_1()
{
  err_r * e = test_function_2();
  if (e) {
    return err_return(ERR_FAILURE, "test_function_2() failed");
  }
  return NULL;
}

BT_TEST(error, plain)
{
  printf("[err] calling failing functions\n");
  err_r * e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  printf("[err] printing error report\n");
  err_report(STDOUT_FILENO);
  err_reset();

  printf("[err] calling failing function\n");
  e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  printf("[err] resetting error status\n");
  err_reset();
  printf("[err] checking that reset worked\n");
  bt_assert_ptr_equal(err_pop(), NULL);
  bt_assert_ptr_equal(err_pop(), NULL);

  return BT_RESULT_OK;
}

#endif /* TEST */
