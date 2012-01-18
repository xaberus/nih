#define _hash_rot(x, k) \
  (((x) << (k)) | ((x) >> (32 - (k))))

#define _hash_mix(a, b, c) \
  { \
    a -= c;  a ^= _hash_rot(c, 4);  c += b; \
    b -= a;  b ^= _hash_rot(a, 6);  a += c; \
    c -= b;  c ^= _hash_rot(b, 8);  b += a; \
    a -= c;  a ^= _hash_rot(c, 16);  c += b; \
    b -= a;  b ^= _hash_rot(a, 19);  a += c; \
    c -= b;  c ^= _hash_rot(b, 4);  b += a; \
  }

#define _hash_final(a, b, c) \
  { \
    c ^= b; c -= _hash_rot(b, 14); \
    a ^= c; a -= _hash_rot(c, 11); \
    b ^= a; b -= _hash_rot(a, 25); \
    c ^= b; c -= _hash_rot(b, 16); \
    a ^= c; a -= _hash_rot(c, 4); \
    b ^= a; b -= _hash_rot(a, 14); \
    c ^= b; c -= _hash_rot(b, 24); \
  }

inline static
uint32_t hash(const void * key, size_t length, uint32_t initval)
{
  uint32_t a, b, c;

  a = b = c = 0xdeadbeef + ((uint32_t) length) + initval;

  {
    const uint32_t * k = (const uint32_t *) key;

    while (length > 12) {
      a += k[0];
      b += k[1];
      c += k[2];
      _hash_mix(a, b, c);
      length -= 12;
      k += 3;
    }
    if (length > 0) {
      uint32_t buff[3] = {0};
      memcpy(buff, k, length);
      a += buff[0];
      b += buff[1];
      c += buff[2];
      _hash_mix(a, b, c);
      _hash_final(a, b, c);
    }
  }

  return c;
}