#ifndef __ERR_H__
#define __ERR_H__

#include <stdint.h>
#include <errno.h>

#define bool int

typedef uint32_t err_t;

enum err_type {
  ERR_SUCCESS = 0,
  ERR_FAILURE,
  ERR_CORRUPTION,

  ERR_PROGRESS,
  ERR_ERRNO,

  /*  argument passing */
  ERR_IN_NULL_POINTER,
  ERR_IN_OVERFLOW,
  ERR_IN_INVALID,
  ERR_IN_DIVIDE_BY_ZERO,

  /* genral errors */
  ERR_INVALID,
  ERR_INVALID_INPUT,
  ERR_UNHANDLED_INPUT,
  ERR_NOT_INITIALIZED,
  ERR_NOT_SET,

  /* calculation errors */
  ERR_OVERFLOW,
  ERR_DIVIDE_BY_ZERO,

  /* memory (de)allocation */
  ERR_MEM_ALLOC,
  ERR_MEM_USE_ALLOC,
  ERR_MEM_REALLOC,
  ERR_MEM_FREE,

  /* index errors */
  ERR_NOT_FOUND,
  ERR_DUPLICATE,
  ERR_BUFFER_OVERFLOW,

  /* io errors */
  ERR_NOT_FD,
  ERR_NOT_FILE,
  ERR_NOT_DIRECTORY,
  ERR_NOT_WRITEABLE,
  ERR_NOT_READABLE,

  /* sx */
  ERR_SX_UNHANDLED,
  ERR_SX_PAREN_MISMATCH,
  ERR_SX_MALFORMED_ATOM,
  ERR_SX_UTF8_ERROR,
  ERR_SX_SUCCESS,
  ERR_SX_LIST_START_GARBAGE,
  ERR_SX_LIST_END_GARBAGE,
  ERR_SX_ATOM_END_GARBAGE,
};

typedef struct err_r {
  err_t        err;
  const char * file;
  int          line;
  const char * fun;
  const char * msg;
} err_r;

#define err_return(_err, _msg) err_push((_err), __FILE__, __LINE__, __FUNCTION__, (_msg))
err_r * err_push(err_t, const char *, int, const char *, const char *);
err_r * err_pop();
void    err_reset();
void    err_report(int fd);

/* tuple */ typedef struct { err_r * err; size_t size; } e_size_t;
/* tuple */ typedef struct { err_r * err; uint32_t uint32; } e_uint32_t;
/* tuple */ typedef struct { err_r * err; uint64_t uint64; } e_uint64_t;
/* tuple */ typedef struct { err_r * err; void * value; } e_void_t;

#ifdef TEST
#define bt_chkerr(_err) \
  do { \
    err_r * err = (_err); \
    if (err) { \
      err_report(STDOUT_FILENO); \
      dprintf(STDOUT_FILENO, "->%s:%s:%d\n", \
          __FILE__, __FUNCTION__, __LINE__); \
      err_reset(); \
      return BT_RESULT_FAIL; \
    } \
  } while(0)
#endif /* TEST */

#endif /* __ERR_H__ */
