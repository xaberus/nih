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


typedef void (err_log_t)(void *, char format[], ...);

extern err_log_t * err_logger;
extern void      * err_logger_data;

#ifdef TEST
#define bt_chkerr(__err) bt_assert_int_equal(__err, 0)
#endif /* TEST */

#endif /* __ERR_H__ */
