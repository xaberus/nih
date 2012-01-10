#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common/err.h"

#define ERR_MAXSTACK 128

static __thread
struct {
  uint8_t used;
  err_r   buf[ERR_MAXSTACK];
} errorstack = {
  .used = 0,
  .buf = {{ERR_SUCCESS, NULL, 0, NULL, NULL}},
};

err_r * err_push(err_t err, const char * fi, int li, const char * fn, const char * m)
{
  if (errorstack.used >= ERR_MAXSTACK) {
    err_report(STDERR_FILENO);
    abort();
  }
  err_r * r = &errorstack.buf[errorstack.used++];
  *r = (err_r) {
    .err = err,
    .file = fi,
    .line = li,
    .fun = fn,
    .msg = m,
  };
  return r;
}

void err_reset()
{
  errorstack.used = 0;
}

err_r * err_pop()
{
  if (errorstack.used > 0) {
    errorstack.used--;
    return &errorstack.buf[errorstack.used];
  }
  return NULL;
}

void err_report(int fd)
{
  int df = dup(fd);
  uint8_t used = errorstack.used;
  if (df != -1) {
    FILE * st = fdopen(df, "w");
    if (st) {
      fprintf(st, "Error\n");
      for (unsigned k = 0; k < used; k++) {
        err_r * e = &errorstack.buf[k];
        fprintf(st, "  %s:%d:%s: %s\n", e->file, e->line, e->fun, e->msg);
        used--;
      }
      fclose(st);
    } else {
      fprintf(stderr, "Error (could not open fd)\n");
      while (used) {
        err_r * e = &errorstack.buf[used - 1];
        fprintf(stderr, "  %s:%d:%s: %s\n", e->file, e->line, e->fun, e->msg);
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

# include <bt.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>

BT_SUITE_DEF(error, "error handling tests");

static
err_r * test_function_5()
{
  if (1) {
    return err_return(ERR_FAILURE, "inner complains");
  }
  return NULL;
}

static
err_r * test_function_4()
{
  err_r * e = test_function_5();
  if (e) {
    return err_return(ERR_FAILURE, "complains about fn_5()");
  }
  return NULL;
}

static
err_r * test_function_3()
{
  err_r * e = test_function_4();
  if (e) {
    return err_return(ERR_FAILURE, "complains about fn_4()");
  }
  return NULL;
}

static
err_r * test_function_2()
{
  err_r * e = test_function_3();
  if (e) {
    return err_return(ERR_FAILURE, "complains about fn_3()");
  }
  return NULL;
}

static
err_r * test_function_1()
{
  err_r * e = test_function_2();
  if (e) {
    return err_return(ERR_FAILURE, "complains about fn_2()");
  }
  return NULL;
}

BT_TEST_DEF_PLAIN(error, plain, "simple tests")
{
  err_r * e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  do {
    bt_log("test: %s:%d:%s: %s\n", e->file, e->line, e->fun, e->msg);
    e = err_pop();
  } while(e);

  e = test_function_1();
  bt_assert_ptr_not_equal(e, NULL);
  err_reset();
  bt_assert_ptr_equal(err_pop(), NULL);

  return BT_RESULT_OK;
}

#endif /* TEST */
