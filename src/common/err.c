#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "common/err.h"

#define ERR_MAXSTACK 128

typedef struct {
  uint8_t used;
  err_r   buf[ERR_MAXSTACK];
} errorstack_t;

static __thread errorstack_t * estack = NULL;

static
err_r internal_err_mem_alloc = {
  .err = ERR_MEM_ALLOC,
  .file = 0,
  .line = 0,
  .fun = 0,
  .msg = "could not allocate error stack",
  .eno = 0,
};

static
err_r internal_err_overflow = {
  .err = ERR_MEM_ALLOC,
  .file = 0,
  .line = 0,
  .fun = 0,
  .msg = "error stack overflow",
  .eno = 0,
};

err_r * err_push(err_t err, const char * fi, int li, const char * fn, const char * m)
{
  if (!estack) {
    estack = malloc(sizeof(errorstack_t));
    if (!estack) {
      assert(0);
      return &internal_err_mem_alloc;
    }
    memset(estack, 0, sizeof(errorstack_t));
  }
  if (estack->used >= ERR_MAXSTACK) {
    err_report(STDERR_FILENO);
    assert(0);
    return &internal_err_overflow;
  }
  err_r * r = &estack->buf[estack->used++];
  *r = (err_r) {
    .err = err,
    .file = fi,
    .line = li,
    .fun = fn,
    .msg = m,
    .eno = errno,
  };
  errno = 0;
  return r;
}

void err_reset()
{
  free(estack);
  estack = NULL;
}

err_r * err_pop()
{
  if (estack && estack->used > 0) {
    estack->used--;
    return &estack->buf[estack->used];
  } else {
    free(estack);
    estack = NULL;
  }
  return NULL;
}

void err_report(int fd)
{
  int df = dup(fd);
  uint8_t used = estack->used;
  if (df != -1) {
    FILE * st = fdopen(df, "w");
    if (st) {
      fprintf(st, "Error report (depth: %u)\n", used);
      for (unsigned k = 0; k < used; k++) {
        err_r * e = &estack->buf[k];
        fprintf(st, "  %s() @ %s:%d\n    %s\n", e->fun, e->file, e->line, e->msg);
      }
      fclose(st);
    } else {
      fprintf(stderr, "Error report(could not open fd)\n");
      while (used) {
        err_r * e = &estack->buf[used - 1];
        fprintf(stderr, "  %s() @ %s:%d\n    %s\n", e->fun, e->file, e->line, e->msg);
        used--;
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

BT_SUITE_DEF(error, "error handling tests");

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

BT_TEST_DEF_PLAIN(error, plain, "simple tests")
{
  bt_log("[err] calling failing functions\n");
  err_r * e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  bt_log("[err] printing error report\n");
  err_report(STDOUT_FILENO);
  err_reset();

  bt_log("[err] calling failing function\n");
  e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  bt_log("[err] resetting error status\n");
  err_reset();
  bt_log("[err] checking that reset worked\n");
  bt_assert_ptr_equal(err_pop(), NULL);

  return BT_RESULT_OK;
}

#endif /* TEST */
