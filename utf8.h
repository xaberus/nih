#include <stdint.h>

static const uint8_t utf8d[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 00..0f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 10..1f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 20..2f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 30..3f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 40..4f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 50..5f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 60..6f
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                  // 70..7f
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                  // 80..8f
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,                  // 90..9f
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,                  // a0..af
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,                  // b0..bf
  8, 8, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,                  // c0..cf
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,                  // d0..df
  0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x4, 0x3, 0x3, // e0..ef
  0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, // f0..ff
  0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4, 0x6, 0x1, 0x1, 0x1, 0x1, // s0..s0
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                  // s1..s1
  1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 1,                  // s2..s2
  1, 2, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1,                  // s3..s3
  1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1,                  // s4..s4
  1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1,                  // s5..s5
  1, 1, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1,                  // s6..s6
  1, 3, 1, 1, 1, 1, 1, 3, 1, 3, 1, 1, 1, 1, 1, 1,                  // s7..s7
  1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                  // s8..s8
};

static inline
uint32_t utf8_decode(uint32_t * state, uint32_t * codep, uint8_t byte)
{
  // fprintf(stderr, "I: 0x%02hhx '%c'\n", byte, isprint(byte) ? byte : '.');
  uint32_t type = utf8d[byte];

  *codep = (*state != 0) ?
           (byte & 0x3fu) | (*codep << 6) :
           (0xff >> type) & (byte);

  *state = utf8d[256 + *state * 16 + type];
  return *state;
}

static inline
unsigned int utf8_encode(uint64_t u, uint8_t b[6])
{
  if (u < 128) {
    b[0] = (uint8_t) u;
    return 1;
  } else if (u < 2047) {
    b[0] = (uint8_t) (192 + (u / 64));
    b[1] = (uint8_t) (128 + (u % 64));
    return 2;
  } else if (u < 65535) {
    b[0] = (uint8_t) (224 + (u / 4096));
    b[1] = (uint8_t) (128 + ((u / 64) % 64));
    b[2] = (uint8_t) (128 + (u % 64));
    return 3;
  } else if (u < 2097151) {
    b[0] = (uint8_t) (240 + (u / 262144));
    b[1] = (uint8_t) (128 + ((u / 4096) % 64));
    b[2] = (uint8_t) (128 + ((u / 64) % 64));
    b[3] = (uint8_t) (128 + (u % 64));
    return 4;
  } else if (u < 67108863) {
    b[0] = (uint8_t) (248 + (u / 16777216));
    b[1] = (uint8_t) (128 + ((u / 262144) % 64));
    b[2] = (uint8_t) (128 + ((u / 4096) % 64));
    b[3] = (uint8_t) (128 + ((u / 64) % 64));
    b[4] = (uint8_t) (128 + (u % 64));
    return 5;
  } else if (u < 2147483647) {
    b[0] = (uint8_t) (248 + (u / 1073741824));
    b[1] = (uint8_t) (248 + ((u / 16777216) % 64));
    b[2] = (uint8_t) (128 + ((u / 262144) % 64));
    b[3] = (uint8_t) (128 + ((u / 4096) % 64));
    b[4] = (uint8_t) (128 + ((u / 64) % 64));
    b[5] = (uint8_t) (128 + (u % 64));
    return 6;
  }
  return 0;
}

enum urs {
  UR_CONT =  0x00,
  UR_ERROR = 0x01,
  UR_EOB   = 0x02,
};

typedef struct {
  uint8_t  m;
  uint8_t  l;
  uint32_t s;
  uint32_t c;
  uint8_t  r[6];
} ur_t;

static inline
const char * ur_read(const char * br, const char * be, ur_t * u)
{
  char          c;

  if (u->s == 1)
    goto read_error;

  if (u->s == 0)
    u->l = 0;

  if (br < be) {
    while (br < be) {
      c = *(br++);
      if (utf8_decode(&u->s, &u->c, c) != 0) {
        if (u->s != 1) {
          u->r[u->l++] = c;
          continue;
        } else {
          goto read_error;
        }
      }
      u->r[u->l++] = c;
      // fprintf(stderr, "    O: 0x%04x '%.*s'\n", u->c, u->l, u->r);
      break;
    }
    /* we obviusly could not read the whole sequence */
    if (u->s > 1) {
      u->m = UR_EOB;
    }
  } else {
    u->m = UR_EOB;
  }

  return br;
read_error:
  u->m = UR_ERROR;
  return NULL;
}

