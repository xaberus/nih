#include <stdint.h>

/*
 * TODO: check if this compies with:
 *  http://www.unicode.org/versions/Unicode5.0.0/ch03.pdf#G7404
 */


static inline
unsigned int sx_parser_encode_utf8(uint64_t u, uint8_t b[6])
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
