#ifndef _SDISK_H
#define _SDISK_H

#include <stdint.h>

#define SDISK_SZU8  1
#define SDISK_SZU16 2
#define SDISK_SZU32 4
#define SDISK_SZU64 8
#define SDISK_SZDBL 8

#define SDISK_SZRID SDISK_SZU32

#define ALIGN2(_size) (((_size) + 1L) & ~1L)
#define ALIGNDATA(_size) (((_size) + (STORE_DATACHUNK-1)) & ~(STORE_DATACHUNK-1))

/* on disk format */

inline static
uint8_t * sdisk_u8_write(uint8_t * dst, uint8_t value)
{
  *((uint8_t *) dst) = value;
  return dst + SDISK_SZU8;
}

inline static
uint16_t sdisk_u8_read(uint8_t ** dstp)
{
  uint16_t value = *((uint8_t *) *dstp);
  *dstp += SDISK_SZU8;
  return value;
}

inline static
uint8_t * sdisk_u16_write(uint8_t * dst, uint16_t value)
{
  *((uint16_t *) dst) = value;
  return dst + SDISK_SZU16;
}

inline static
uint16_t sdisk_u16_read(uint8_t ** dstp)
{
  uint16_t value = *((uint16_t *) *dstp);
  *dstp += SDISK_SZU16;
  return value;
}

inline static
uint8_t * sdisk_i32_write(uint8_t * dst, int32_t value)
{
  *((int32_t *) dst) = value;
  return dst + SDISK_SZU32;
}

inline static
int32_t sdisk_i32_read(uint8_t ** dstp)
{
  int32_t value = *((int32_t *) *dstp);
  *dstp += SDISK_SZU32;
  return value;
}

inline static
uint8_t * sdisk_u32_write(uint8_t * dst, uint32_t value)
{
  *((uint32_t *) dst) = value;
  return dst + SDISK_SZU32;
}

inline static
uint32_t sdisk_u32_read(uint8_t ** dstp)
{
  uint32_t value = *((uint32_t *) *dstp);
  *dstp += SDISK_SZU32;
  return value;
}

#define sdisk_rid_write(_d, _v) sdisk_u32_write((_d), (_v))
#define sdisk_rid_read(_d) sdisk_u32_read(_d)

inline static
uint8_t * sdisk_i64_write(uint8_t * dst, int64_t value)
{
  *((int64_t *) dst) = value;
  return dst + SDISK_SZU64;
}

inline static
int64_t sdisk_i64_read(uint8_t ** dstp)
{
  int64_t value = *((int64_t *) *dstp);
  *dstp += SDISK_SZU64;
  return value;
}

inline static
uint8_t * sdisk_u64_write(uint8_t * dst, uint64_t value)
{
  *((uint64_t *) dst) = value;
  return dst + SDISK_SZU64;
}

inline static
uint64_t sdisk_u64_read(uint8_t ** dstp)
{
  uint64_t value = *((uint64_t *) *dstp);
  *dstp += SDISK_SZU64;
  return value;
}

inline static
uint8_t * sdisk_dbl_write(uint8_t * dst, double value)
{
  *((double *) dst) = value;
  return dst + SDISK_SZDBL;
}

inline static
double sdisk_dbl_read(uint8_t ** dstp)
{
  double value = *((double *) *dstp);
  *dstp += SDISK_SZDBL;
  return value;
}

inline static
uint8_t * sdisk_obj_write(uint8_t * dst, smrec_t * o)
{
  if (o) {
    return sdisk_rid_write(dst, o->id);
  } else {
    return sdisk_rid_write(dst, SRID_NIL);
  }
}

inline static
uint8_t * sdisk_cls_write(uint8_t * dst, sclass_t * c)
{
  if (c) {
    return sdisk_rid_write(dst, c->id);
  } else {
    return sdisk_rid_write(dst, SRID_NIL);
  }
}

inline static
uint16_t sdisk_size(skind_t kind)
{
  switch (kind) {
    case SKIND_NONE:
      return 0;
    case SKIND_UINT8:
      return SDISK_SZU8;
    case SKIND_UINT16:
      return SDISK_SZU16;
    case SKIND_INT32:
    case SKIND_UINT32:
      return SDISK_SZU32;
    case SKIND_INT64:
    case SKIND_UINT64:
      return SDISK_SZU64;
    case SKIND_DOUBLE:
      return SDISK_SZDBL;
    case SKIND_OBJECT:
    case SKIND_ODREF:
    case SKIND_CLASS:
      return SDISK_SZRID;
  }
  return 0;
}

#endif /* _SDISK_H */