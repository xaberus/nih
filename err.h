#ifndef __ERR_H__
#define __ERR_H__

#include <stdint.h>
#include <errno.h>

union err {
  __extension__ struct {
    uint8_t major;
    uint8_t minor;
    uint16_t extra;
  };
  uint32_t composite;
};
typedef union err err_t;

enum {
  ERR_MAJ_SUCCESS = 0,
  ERR_MAJ_ERRNO,
  ERR_MAJ_NULL_POINTER,
  ERR_MAJ_OVERFLOW,
  ERR_MAJ_INVALID,
  ERR_MAJ_ALLOC,
  ERR_MAJ_IO,
};

enum {
  ERR_MIN_SUCCESS = 0,
  ERR_MIN_IN_NULL_POINTER,
  ERR_MIN_OUT_NULL_POINTER,
  ERR_MIN_IN_OVERFLOW,
  ERR_MIN_MID_OVERFLOW,
  ERR_MIN_CALC_OVERFLOW,
  ERR_MIN_OUT_OVERFLOW,
  ERR_MIN_BUFFER_OVERFLOW,
  ERR_MIN_IN_INVALID,
  ERR_MIN_NOT_FOUND,
  ERR_MIN_NOT_INITIALIZED,
  ERR_MIN_NOT_SET,
  ERR_MIN_ALLOC,
  ERR_MIN_POOL_ALLOC,
  ERR_MIN_REALLOC,
  ERR_MIN_FREE,
  ERR_MIN_IO_NO_FILE,
};

typedef void (err_log_t)(void *, char format[], ...);

extern err_log_t * err_logger;
extern void      * err_logger_data;


static inline
err_t err_construct(uint8_t major, uint8_t minor, uint16_t extra)
{
  err_t err = {{
                 .major = major,
                 .minor = minor,
                 .extra = extra,
               }};

  return err;
}

static inline
err_t err_from_errno()
{
  err_t err = {{
                 .major = ERR_MAJ_ERRNO,
                 .minor = (errno >> 16) & (0xFF),
                 .extra = (errno & 0xFFFF),
               }};

  return err;
}

static inline
err_t reconstruct_error(err_t err, uint16_t extra)
{
  err.extra = extra;
  return err;
}

#endif /* __ERR_H__ */
