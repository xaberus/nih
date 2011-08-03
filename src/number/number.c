#include "number.h"

#define ALIGN32(_size) (((_size) + 31L) & ~31L)

gc_vtable_t number_vtable = {
  .flag = GC_VT_FLAG_HDR,
  .gc_init = NULL,
  .gc_clear = NULL,
};

number_t * number_new(gc_global_t * g, uint64_t bits)
{
  uint64_t s = ALIGN32(bits) >> 5; /* bits / 32 */
  uint64_t b = s << 2; /* s * 4 */

  if (s > INT32_MAX || b > GC_SIZE_MAX) {
    gc_error(g, "number: cannot store %Qu bits", bits);
    return NULL;
  }

  number_t * n = gc_new(g, &number_vtable, sizeof(number_t) + 4 * b);
  n->s = 0;

  return n;
}

inline static
uint32_t number_size(number_t * n)
{
  return ((((GC_HDR(n)->flag & 0xffffff00) >> 8)) - sizeof(number_t)) >> 2;
}

inline static
void intern_set_bin(number_t * n, int sign, size_t len, const char str[len])
{
  const char * p;
  uint8_t      u, m;
  char         c;
  uint64_t     r;
  uint32_t   * d = n->data;
  uint32_t     s = 0;

  for (p = str + len - 1, m = 0, r = 0; p >= str; p--) {
    if ('0' <= (c = *p) && c <= '1') {
      u = c - '0';
    } else {
      continue;
    }

    r |= (uint64_t) u << m; m++;
    if (m == 32) {
      *(d++) = (uint32_t) r; s++; m = 0; r = 0;
    }
  }

  if (m) {
    *(d++) = r; s++;
  }

  n->s = s * sign;
}

inline static
void intern_set_oct(number_t * n, int sign, size_t len, const char str[len])
{
  const char * p;
  uint8_t      u, m;
  char         c;
  uint64_t     r;
  uint32_t   * d = n->data;
  uint32_t     s = 0;

  for (p = str + len - 1, m = 0, r = 0; p >= str; p--) {
    if ('0' <= (c = *p) && c <= '7') {
      u = c - '0';
    } else {
      continue;
    }

    r |= (uint64_t) u << m; m += 3;
    if (m >= 32) {
      *(d++) = (uint32_t) r; m -= 32; r >>= 32; s++;
    }
  }

  while (r) {
    *(d++) = r; r >>= 32; s++;
  }

  n->s = s * sign;
}

inline static
uint32_t intern_mul_one(uint32_t u, uint32_t rd[u], uint32_t ad[u], uint32_t b)
{
  uint32_t * lim = ad + u; /* must be here */
  uint64_t   r = 0;

  do {
    r = r + (uint64_t) *(ad++) * (uint64_t) b;
    *(rd++) = (uint32_t) r; r >>= 32;
  } while (--u != 0);

  if (rd != ad) {
    while (ad < lim) {
      *(rd++) = *(ad++);
    }
  }

  return r;
}

inline static
uint32_t intern_add_one(uint32_t u, uint32_t rd[u], uint32_t ad[u], uint32_t b)
{
  uint32_t * lim = ad + u; /* must be here */
  uint64_t   r;

  r = (uint64_t) *(ad++) + (uint64_t) b; u--;
  *(rd++) = (uint32_t) r; r >>= 32;

  while (r && u) {
    r = r + (uint64_t) *(ad++); u--;
    *(rd++) = (uint32_t) r; r >>= 32;
  }

  if (rd != ad) {
    while (ad < lim) {
      *(rd++) = *(ad++);
    }
  }
  return r;
}

inline static
void intern_set_dec(number_t * n, int sign, size_t len, const char str[len])
{
  uint32_t     s;
  uint32_t     b;
  long         j;
  const char * p, * pe;
  char         c;
  uint8_t      u;
  uint32_t   * d = n->data;
  uint32_t     r, m;

  for (s = 0, r = 0, j = 9, b = 1, p = str, pe = str + len; p < pe; p++) {
    if ('0' <= (c = *(p)) && c <= '9') {
      u = (c - '0');
    } else {
      continue;
    }

    r = r * 10 + u; b *= 10; j--;
    if (j == 0) {
      if (s == 0) {
        if (r) {
          d[0] = r; s++;
        }
      } else {
        m = intern_mul_one(s, d, d, b);
        m += intern_add_one(s, d, d, r);
        if (m) {
          d[s++] = m;
        }
      }
      j = 9;
      b = 1;
      r = 0;
    }
  }

  if (j!= 9) {
    if (s == 0) {
      if (r) {
        d[0] = r; s++;
      }
    } else {
      m = intern_mul_one(s, d, d, b);
      m += intern_add_one(s, d, d, r);
      if (m) {
        d[s++] = m;
      }
    }
  }

  n->s = s * sign;
}

inline static
void intern_set_hex(number_t * n, int sign, size_t len, const char str[len])
{
  const char * p;
  uint8_t      u, m;
  char         c;
  uint64_t     r;
  uint32_t   * d = n->data;
  uint32_t     s = 0;

  for (p = str + len - 1, m = 0, r = 0; p >= str; p--) {
    if ('0' <= (c = *p) && c <= '9') {
      u = c - '0';
    } else if ('a' <= c && c <= 'f') {
      u = c - 'a' + 10;
    } else if ('A' <= c && c <= 'F') {
      u = c - 'A' + 10;
    } else {
      continue;
    }

    r |= (uint64_t) u << (m << 2); m++;
    if (m == 8) {
      *(d++) = (uint32_t) r; s++; m = 0; r = 0;
    }
  }

  if (m) {
    *(d++) = r; s++;
  }

  n->s = s * sign;
}

number_t * number_setstrc(gc_global_t * g, number_t * n, size_t len, const char str[len])
{
  const char * p = str, * pe = p + len, * ps;
  char         c;
  int          sign = 1;
  unsigned     form;
  uint64_t     digits = 0;

  enum {
    ERROR = 0, START = 1, NUMBER0, NUMBERX, BIN, BINC, OCT, OCTC, HEX, HEXC, DEC, DECC,
  } cs = START;

#define TC(_state) do { cs = _state; p++; goto loop; } while (0)
#define TA(_state) do { cs = _state; goto loop; } while (0)

loop:
  if (p < pe) {
    switch (cs) {
      case START: {
        switch ((c = *p)) {
          case '+': { sign = +1; TC(NUMBER0); }
          case '-': { sign = -1; TC(NUMBER0); }
          case '0': { ps = p; TC(NUMBERX); }
          case ' ':
          case '_': { TC(START); }
          default: {
            if ('1' <= c && c <= '9') { TA(DEC); }
          } goto abort;
        }
      }
      case NUMBER0: {
        switch ((c = *p)) {
          case '0': { ps = p; TC(NUMBERX); }
          case ' ':
          case '_': { TC(NUMBER0); }
          default: {
            if ('1' <= c && c <= '9') { TA(DEC); }
          } goto abort;
        }
      } goto abort;
      case NUMBERX: {
        switch ((c = *p)) {
          case 'b': { form = ERROR; TC(BIN); }
          case 'o': { form = ERROR; TC(OCT); }
          case 'x': { form = ERROR; TC(HEX); }
          case ' ':
          case '_': { TC(NUMBERX); }
          default: {
            if ('0' <= c && c <= '9') { TA(DEC); }
          } goto abort;
        }
      } goto abort;
      case BIN: {
        switch ((c = *p)) {
          case ' ':
          case '_':
          case '0': { TC(BIN); }
          case '1': { form = BIN; ps = p; digits++; TC(BINC); }
        }
      } goto abort;
      case BINC: {
        switch ((c = *p)) {
          case '0': { digits++; } TC(BINC);
          case '1': { digits++; } TC(BINC);
          case ' ':
          case '_': { TC(BINC); }
        }
      } goto abort;
      case HEX: {
        switch ((c = *p)) {
          case ' ':
          case '_':
          case '0': { TC(HEX); }
          case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
          case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
          case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          { form = HEX; ps = p; digits++; TC(HEXC); }
        }
      } goto abort;
      case HEXC: {
        switch ((c = *p)) {
          case '0':
          case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
          case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
          case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          { digits++; TC(HEXC); }
          case ' ':
          case '_': { TC(HEXC); }
        }
      } goto abort;
      case OCT: {
        switch ((c = *p)) {
          case ' ':
          case '_':
          case '0': { TC(OCT); }
          case '1': case '2': case '3': case '4': case '5': case '6': case '7':
          { form = OCT; ps = p; digits++; TC(OCTC); }
        }
      } goto abort;
      case OCTC: {
        switch ((c = *p)) {
          case '0': { TC(OCTC); }
          case '1': case '2': case '3': case '4': case '5': case '6': case '7':
          { digits++; TC(OCTC); }
          case ' ':
          case '_': { TC(OCTC); }
        }
      } goto abort;
      case DEC: {
        switch ((c = *p)) {
          case ' ':
          case '_':
          case '0': { TC(DEC); }
          case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
          { form = DEC; ps = p; digits++; TC(DECC); }
        }
      } goto abort;
      case DECC: {
        switch ((c = *p)) {
          case '0':
          case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
          { digits++; TC(DECC); }
          case ' ':
          case '_': { TC(DECC); }
        }
      } goto abort;
      case ERROR: { goto abort; }
    }
  }
  if (digits) {
    uint64_t bits;

#define PREPARE(_g, _n, _bits) \
  do { \
    if (!(_n)) { \
      (_n) = number_new((_g), (_bits)); \
    } else if ((number_size(n) << 5) < (_bits)) { \
      (_n) = number_new((_g), (_bits)); \
    } \
  } while (0)

    switch (form) {
      case BIN: {
        bits = ALIGN32(digits);
        PREPARE(g, n, bits);
        intern_set_bin(n, sign, len - (ps - str), ps);
        return n;
      }
      case OCT: {
        bits = ALIGN32(digits * 3);
        PREPARE(g, n, bits);
        intern_set_oct(n, sign, len - (ps - str), ps);
        return n;
      }
      case DEC: {
        const double cpb = 0.3010299956639812;
        bits = (digits / cpb);
        PREPARE(g, n, bits);
        intern_set_dec(n, sign, len - (ps - str), ps);
        return n;
      }
      case HEX: {
        bits = ALIGN32(digits * 4);
        PREPARE(g, n, bits);
        intern_set_hex(n, sign, len - (ps - str), ps);
        return n;
      }
      default: { goto abort; }
    }
  } else {
    PREPARE(g, n, 32);
    n->s = 0;
    return n;
  }
abort:
  return NULL;
}

number_t * number_sethex(gc_global_t * g, number_t * n, gc_str_t * hex)
{
  return number_setstrc(g, n, gc_str_len(hex), hex->data);
}

inline static
uint32_t iabs(int32_t i)
{
  return i > 0 ? i : -i;
}

#define alloc_buffer(_size) \
  (void *) (((gc_hdr_t *) gc_new(g, &gc_blob_vtable, sizeof(gc_hdr_t) + (_size))) + 1)

gc_str_t * number_gethex(gc_global_t * g, number_t * n)
{
  uint32_t * d = n->data;
  uint32_t   len = 2;
  uint32_t   s = iabs(n->s);
  char     * buf, * p;

  if (n->s <= 0) {
    len = 3;
  }

  len += s * 8;

  buf = alloc_buffer(len + 1);
  p = buf;

  if (n->s < 0) {
    *(p++) = '-';
    *(p++) = '0';
    *(p++) = 'x';
  } else if (s == 0) {
    *(p++) = '0';
    *(p++) = 'x';
    *(p++) = '0';
  } else {
    *(p++) = '0';
    *(p++) = 'x';
  }

  if (s) {
    uint32_t i = d[s - 1], m = 0;
    uint8_t  nib;

    if (i & 0xf0000000) {
      m = 28;
    } else if (i & 0xf000000) {
      m = 24;
    } else if (i & 0xf00000) {
      m = 20;
    } else if (i & 0xf0000) {
      m = 16;
    } else if (i & 0xf000) {
      m = 12;
    } else if (i & 0xf00) {
      m = 8;
    } else if (i & 0xf0) {
      m = 4;
    } else if (i & 0xf) {
      m = 0;
    }

    for (uint32_t k = s; k > 0; k--, m = 28) {
      i = d[k - 1];

      for (uint32_t j = 0, l = m / 4 + 1; j < l; j++, m -= 4) {
        nib = (i & (0xf << m)) >> m;
        *(p++) = nib < 10 ? nib + '0' : nib - 10 + 'a';
      }
    }
  }

  return gc_new_str(g, p - buf, buf);
}

static const
uint8_t lbz_tab[128] = {
  1, 2, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8
};


/* lbz = leading bit zeros  */
uint32_t intern_lbz(uint32_t msl)
{

  uint32_t a = msl < (0x10000)
               ? (msl < (0x100) ? 1 : 8 + 1)
               : (msl < (0x1000000) ? 16 + 1 : 24 + 1);

  return 32 + 1 - a - lbz_tab[msl >> a];
}

gc_str_t * number_getdec(gc_global_t * g, number_t * n)
{
  uint32_t s = iabs(n->s);

  if (s == 0) {
    return gc_new_str(g, 1, "0");
  }

  uint64_t bits = (uint64_t) s * 32 - intern_lbz(n->data[s - 1]);
  uint64_t sz = (uint64_t) (bits * 0.3010299956639811) + 1;

  if (n->s < 0) {
    sz++;
  }

  uint32_t * tn = alloc_buffer((s) * sizeof(uint32_t));
  memcpy(tn, n->data, s * sizeof(uint32_t));

  char * buf = alloc_buffer(sz + 1), * p = buf + sz;

  uint64_t r;

  while (s > 1) {
    uint32_t * d = tn + s - 1;
    r = 0;
    do {
      r = (r << 32) | *d;
      *(d--) = r / 1000000000;
      r = r % 1000000000;
    } while (d >= tn);
    s -= tn[s - 1] == 0;
    for (unsigned k = 0; k < 9; k++) {
      *(--p) = (r % 10) + '0';
      r /= 10;
    }
  }

  r = tn[0];
  while (r != 0) {
    *(--p) = (r % 10) + '0';
    r /= 10;
  }

  if (n->s < 0) {
    *(--p) = '-';
  }

  return gc_new_str(g, sz - (p - buf), p);
}

inline static
uint32_t max(uint32_t a, uint32_t b)
{
  return a > b ? b : a;
}

#define swap(_a, _b) do { __typeof__(_a) _t = (_a); _a = (_b); _b = _t; } while (0)

inline static
uint32_t intern_size(uint32_t s, uint32_t d[s])
{
  while (s > 0) {
    if (d[s - 1] != 0) {
      break;
    }
    s--;
  }
  return s;
}

inline static
uint32_t intern_add_sl(uint32_t u, uint32_t rd[u], uint32_t ad[u], uint32_t bd[u])
{
  uint64_t r = 0;

  do {
    r = (uint64_t) *(ad++) + (uint64_t) *(bd++) + r;
    *(rd++) = (uint32_t) r; r >>= 32;
  } while (--u != 0);

  return (uint32_t) r;
}

inline static
uint32_t intern_add_dl(uint32_t au, uint32_t rd[au], uint32_t ad[au], uint32_t bu, uint32_t bd[bu])
{
  uint32_t * rp = rd + bu, * ap = ad + bu, * lim = ad + au;

  if (bu) {
    if (intern_add_sl(bu, rd, ad, bd)) {
      /* propagate carry */
      do {
        if (ap >= lim)
          return 1;
      } while ((*(rp++) = *(ap++) + 1) == 0);
    }
  }
  if (rp != ap) {
    while (ap < lim) {
      *(rp++) = *(ap++);
    }
  }
  return 0;
}

inline static
uint32_t intern_sub_sl(uint32_t u, uint32_t rd[u], uint32_t ad[u], uint32_t bd[u])
{
  uint32_t r, a, b;
  uint64_t c = 0;

  do {
    a = *(ad++);
    b = *(bd++);
    c = (uint64_t) 0x100000000 + (uint64_t) a - (uint64_t) b - c;
    r = (uint32_t) c;
    c = (c >> 32) ^ 0x1;
    *(rd++) = r;
  } while (--u != 0);

  return (uint32_t) c;
}

inline static
uint32_t intern_sub_dl(uint32_t au, uint32_t rd[au], uint32_t ad[au], uint32_t bu, uint32_t bd[bu])
{
  uint32_t * rp = rd + bu, * ap = ad + bu, * lim = ad + au;

  if (bu) {
    if (intern_sub_sl(bu, rd, ad, bd)) {
      uint32_t t;
      /* propagate carry */
      do {
        if (ap >= lim)
          return 1;
      } while ((*(rp++) = (uint32_t) (t = *(ap++)) - 1), t == 0);
    }
  }
  if (rp != ap) {
    while (ap < lim) {
      *(rp++) = *(ap++);
    }
  }
  return 0;
}

number_t * number_add(gc_global_t * g, number_t * a, number_t * b)
{
  int32_t    as, bs, rs;
  uint32_t   au, bu;
  uint32_t * ad, * bd, * rd;
  uint32_t   t;

  number_t * n;

  as = a->s; au = iabs(as);
  bs = b->s; bu = iabs(bs);

  if (au < bu) {
    swap(as, bs);
    swap(au, bu);
    ad = b->data;
    bd = a->data;
  } else {
    ad = a->data;
    bd = b->data;
  }

  /* |a| >= |b| */

  rs = as + 1;
  n = number_new(g, rs << 5);
  rd = n->data;

  if ((as ^ bs) < 0) { /* (0b1---) ^ (0b0---) = (0b1~~~)  < 0 */
    if (au != bu) {
      intern_sub_dl(au, rd, ad, bu, bd);
      rs = (t = intern_size(au, rd), as < 0) ? -t : t;
    } else if (ad[au - 1] < bd[bu - 1]) {
      intern_sub_sl(au, rd, bd, ad);
      rs = (t = intern_size(au, rd), as >= 0) ? -t : t;
    } else {
      intern_sub_sl(au, rd, ad, bd);
      rs = (t = intern_size(au, rd), as < 0) ? -t : t;
    }
  } else {
    uint32_t c = intern_add_dl(au, rd, ad, bu, bd); rd[au] = c;
    rs = (t = au + c, as < 0) ? -t : t;
  }
  n->s = rs;
  return n;
}

number_t * number_sub(gc_global_t * g, number_t * a, number_t * b)
{
  int32_t    as, bs, rs;
  uint32_t   au, bu;
  uint32_t * ad, * bd, * rd;
  uint32_t   t;

  number_t * n;

  as = a->s;  au = iabs(as);
  bs = -b->s; bu = iabs(bs);

  if (au < bu) {
    swap(as, bs);
    swap(au, bu);
    ad = b->data;
    bd = a->data;
  } else {
    ad = a->data;
    bd = b->data;
  }

  /* |a| >= |b| */

  rs = au + 1;
  n = number_new(g, rs << 5);
  rd = n->data;

  if ((as ^ bs) < 0) { /* (0b1---) ^ (0b0---) = (0b1~~~)  < 0 */
    if (au != bu) {
      intern_sub_dl(au, rd, ad, bu, bd);
      rs = (t = intern_size(au, rd), as < 0) ? -t : t;
    } else if (ad[au - 1] < bd[bu - 1]) {
      intern_sub_sl(au, rd, bd, ad);
      rs = (t = intern_size(au, rd), as >= 0) ? -t : t;
    } else {
      intern_sub_sl(au, rd, ad, bd);
      rs = (t = intern_size(au, rd), as < 0) ? -t : t;
    }
  } else {
    uint32_t c = intern_add_dl(au, rd, ad, bu, bd); rd[au] = c;
    rs = (t = au + c, as < 0) ? -t : t;
  }
  n->s = rs;
  return n;
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

BT_SUITE_DEF(number, "number (big int) tests");

BT_SUITE_SETUP_DEF(number, objectref)
{
  gc_global_t   * g = malloc(sizeof(gc_global_t));
  mem_allocator_t a = {
    .realloc = plain_realloc,
    .ud = NULL
  };

  bt_assert_ptr_not_equal(g, NULL);

  gc_init(g, a);

  *objectref = g;

  return BT_RESULT_OK;
}

BT_SUITE_TEARDOWN_DEF(number, objectref)
{
  gc_global_t * g = *objectref;

  gc_clear(g);
  free(g);
  return BT_RESULT_OK;
}

BT_TEST_DEF(number, plain, object, "simple tests")
{
  gc_global_t * g = object;

  {
    number_t * n;
    gc_str_t * s;

# define testnum(_A, _B) \
  do { \
    n = number_setstrc(g, NULL, strlen(_A), (_A)); \
    bt_assert_ptr_not_equal(n, NULL); \
    s = number_gethex(g, n); \
    bt_assert_str_equal(s->data, (_B)); \
    bt_log("number_setstrc: %s OK\n", (_A)); \
    gc_collect(g, 1); \
  } while (0)

    testnum("0", "0x0");
    testnum("-0", "0x0");
    testnum("0x0", "0x0");
    testnum("-0x0", "0x0");
    testnum("0o0", "0x0");
    testnum("-0o0", "0x0");
    testnum("0b0", "0x0");
    testnum("-0b0", "0x0");

    testnum("0xaffe", "0xaffe");
    testnum("  0xaffe  ", "0xaffe");
    testnum("0 xaffe  ", "0xaffe");
    testnum(" 0 x a f f e", "0xaffe");
    testnum("- 1 2 3 4 5 6 7", "-0x12d687");

    testnum("-0xaffe", "-0xaffe");
    testnum("0x53777c3b3fa0634e9", "0x53777c3b3fa0634e9");
    testnum("0b1010011011101110111110000111011001111111010000001100011010011101001",
        "0x53777c3b3fa0634e9");
    testnum("0o12335676073177201432351", "0x53777c3b3fa0634e9");
    testnum("12345", "0x3039");
    testnum("123456789", "0x75bcd15");
    testnum("1234567890", "0x499602d2");
    testnum("9623059874062364543635689", "0x7f5c31efe7f61f29550e9");
    testnum("2428709456692296977609751958670727914210672555000931124765335",
        "0x182ea6ddeea13922688e1d6144be59bca3181c7a09f3573e297");
    testnum("7140998891406386129369914639483065433377313489390143087346139",
        "0x471a07327c22bc0d710ed9687b161fca9c987a1f9bb7f3081db");
# undef testnum
  }

  {
    number_t * n;
    gc_str_t * s;

# define testnum(_A, _B) \
  do { \
    n = number_setstrc(g, NULL, strlen(_A), (_A)); \
    bt_assert_ptr_not_equal(n, NULL); \
    s = number_getdec(g, n); \
    bt_assert_str_equal(s->data, (_B)); \
    bt_log("number_setstrc: %s OK\n", (_A)); \
    gc_collect(g, 1); \
  } while (0)

    testnum("0", "0");
    testnum("-0", "0");
    testnum("0x0", "0");
    testnum("-0x0", "0");
    testnum("0o0", "0");
    testnum("-0o0", "0");
    testnum("0b0", "0");
    testnum("-0b0", "0");

    testnum("0xaffe", "45054");
    testnum("-0xaffe", "-45054");
    testnum("0x53777c3b3fa0634e9", "96230598740623635689");
    testnum("12345", "12345");
    testnum("123456789", "123456789");
    testnum("1234567890", "1234567890");

    testnum("-0x7fffffffffffffff", "-9223372036854775807");

    testnum("9623059874062364543635689", "9623059874062364543635689");
    testnum("2428709456692296977609751958670727914210672555000931124765335",
        "2428709456692296977609751958670727914210672555000931124765335");
    testnum("7140998891406386129369914639483065433377313489390143087346139",
        "7140998891406386129369914639483065433377313489390143087346139");
# undef testnum
  }


# define testop(_N, _OP, _A, _B, _C) \
  do { \
    number_t * c, * a, * b; \
    gc_str_t * s; \
    bt_assert_ptr_not_equal((a = number_new(g, (_N))), NULL); \
    bt_assert_ptr_not_equal((a = number_setstrc(g, a, strlen(_A), (_A))), NULL); \
    bt_assert_ptr_not_equal((s = number_gethex(g, a)), NULL); \
    bt_assert_str_equal(s->data, (_A)); \
    bt_assert_ptr_not_equal((b = number_new(g, (_N))), NULL); \
    bt_assert_ptr_not_equal((b = number_setstrc(g, b, strlen(_B), (_B))), NULL); \
    bt_assert_ptr_not_equal((s = number_gethex(g, b)), NULL); \
    bt_assert_str_equal(s->data, (_B)); \
    bt_assert_ptr_not_equal((c = _OP(g, a, b)), NULL); \
    bt_assert_ptr_not_equal((s = number_gethex(g, c)), NULL); \
    bt_assert_str_equal(s->data, (_C)); \
    /*bt_log("number: %s = %s(%s, %s) == %s, OK\n", s->data, #_OP, (_A), (_B), (_C));*/ \
    bt_log("%s: %s == %s, OK\n", # _OP, s->data, (_C)); \
    gc_collect(g, 1); \
  } while (0)

  testop(128, number_add, "0x0", "0x0", "0x0");
  testop(128, number_add, "0x1", "0x1", "0x2");
  testop(128, number_add, "0xffffffff", "0x1", "0x100000000");
  testop(128, number_add, "0xffffffffffffffff", "0xffffffff", "0x100000000fffffffe");
  testop(128, number_add,
      "0x53777c3b3fa0634e9",
      "0x29a2c3dae9b2aa2c3f954a72b7ef7f8",
      "0x29a2c3dae9b2aa7fb71185b25852ce1");

  testop(128, number_sub, "0x0", "0x0", "0x0");
  testop(128, number_sub, "0x1", "0x0", "0x1");
  testop(128, number_sub, "0x0", "0x1", "-0x1");
  testop(128, number_sub, "0x10", "0x5", "0xb");
  testop(128, number_sub,
      "0x29a2c3dae9b2aa7fb71185b25852ce1",
      "0x53777c3b3fa0634e9",
      "0x29a2c3dae9b2aa2c3f954a72b7ef7f8");
  testop(128, number_add,
      "0x29a2c3dae9b2aa7fb71185b25852ce1",
      "-0x53777c3b3fa0634e9",
      "0x29a2c3dae9b2aa2c3f954a72b7ef7f8");
  testop(128, number_sub,
      "0x100000000fffffffe",
      "0xffffffff",
      "0xffffffffffffffff");


  /*bt_assert_ptr_not_equal((n = number_sub(g, a, b)), NULL);
  bt_assert_ptr_not_equal((s = number_gethex(g, n)), NULL);
  bt_assert_str_equal(s->data, "0x0");*/

  return BT_RESULT_OK;
}

#endif /* TEST */
