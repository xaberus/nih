
#include "sr.h"


#define ERR_I_OVER err_construct(ERR_MAJ_OVERFLOW, ERR_MIN_IN_OVERFLOW, 0)
#define ERR_ err_construct(ERR_MAJ_INVALID, ERR_MIN_DIVIDE_BY_ZERO, 0)

unsigned gcd(unsigned a, unsigned b)
{
  if (b == 0)
    return a;

  return gcd(b, a%b);
}

unsigned lcm(unsigned a, unsigned b)
{
  return a*b/gcd(a,b);
}


err_t sr_set(sr_t * rat, int16_t num, uint16_t den)
{
  if (!rat)
    return ERR_IN_NULL_POINTER;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = num;
  d = den;

  if (d == 0)
    return ERR_IN_DIVIDE_BY_ZERO;

  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_OVERFLOW;
  if (d > UINT16_MAX)
    return ERR_OVERFLOW;

  rat->num = n;
  rat->den = d;

  return 0;
}

err_t sr_add(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_IN_NULL_POINTER;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num*b->den + b->num*a->den;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_OVERFLOW;
  if (d > UINT16_MAX)
    return ERR_OVERFLOW;

  result->num = n;
  result->den = d;

  return 0;
}

err_t sr_sub(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_IN_NULL_POINTER;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num*b->den - b->num*a->den;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_OVERFLOW;
  if (d > UINT16_MAX)
    return ERR_OVERFLOW;

  result->num = n;
  result->den = d;

  return 0;
}

err_t sr_mul(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_IN_NULL_POINTER;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num * b->num;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_OVERFLOW;
  if (d > UINT16_MAX)
    return ERR_OVERFLOW;

  result->num = n;
  result->den = d;

  return 0;
}

err_t sr_div(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_IN_NULL_POINTER;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num * b->den;
  d = a->den * b->num;

  if (d == 0)
    return ERR_DIVIDE_BY_ZERO;

  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_OVERFLOW;
  if (d > UINT16_MAX)
    return ERR_OVERFLOW;

  result->num = n;
  result->den = d;

  return 0;
}

#if defined(TEST) && 0
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢╭────────╮▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╮  ╭──╯▢▢▢│ ╭──────╯▢▢▢│ ╭──────╯▢▢▢╰──╮  ╭──╯▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰────╮▢▢▢▢▢│ ╰──────╮▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╭────╯▢▢▢▢▢╰──────╮ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢│  │▢▢▢▢▢▢│ ╰──────╮▢▢▢╭──────╯ │▢▢▢▢▢▢│  │▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢╰──╯▢▢▢▢▢▢╰────────╯▢▢▢╰────────╯▢▢▢▢▢▢╰──╯▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/
/*▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢▢*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "sx.h"

err_t sr_test_eval(sx_t * sx, sr_t * ret)
{
  sr_t a[1] = {{0,1}}, b[1] = {{0,1}};
  err_t err;

  if (!sx)
    return ERR_IN_NULL_POINTER;

  if (sx->islist) {
    sx_t * sn,  * sd;
    signed n = 0;
    unsigned d = 1;

    /* is rational tuple ? */
    if (sx->list) {
      sn = sx->list;
      if (sn->issint || sn->isuint) {
        if (sn->issint)
          n = (int16_t)sn->atom.sint;
        else if (sn->isuint)
          n = (uint16_t)sn->atom.uint;
        else
          return ERR_FAILURE;
        if (sn->next) {
          sd = sn->next;
          if (sd->isuint)
            d = (uint16_t)sd->atom.uint;
          else
            return ERR_FAILURE;
        }
        err = sr_set(ret, n, d);
        if (err)
          return err;
        return 0;
      } else if (sn->isplain) {
        if (strncmp(sn->atom.string->buffer, "add", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err)
                return err;
              if (err)
                return err;
              err = sr_add(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "sub", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err)
                return err;
              if (err)
                return err;
              err = sr_sub(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "mul", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err)
                return err;
              if (err)
                return err;
              err = sr_mul(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "div", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err)
                return err;
              if (err)
                return err;
              err = sr_div(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else
            return ERR_FAILURE;
      }
    }
  }

  return 0;
}

err_t sr_tester(sx_t * sx)
{
  sr_t act[1] = {{0,1}};
  sr_t exp[1] = {{0,1}};
  sx_t * cal;
  sx_t * res;
  err_t err;

  if (sx->islist) {
    sx = sx->list;

    if (!sx)
      return 0;

    cal = sx;
    if (!cal)
      return 0;
    res = cal->next;
    if (!res)
      return 0;

    err = sr_test_eval(cal, act);
    if (err)
      return err;
    err = sr_test_eval(res, exp);
    if (err)
      return err;

    printf("(");
    sx_test_print(cal, 0, 0, 0, 0);
    printf(") == (");
    sx_test_print(res, 0, 0, 0, 0);
    printf(") <=> ");
    printf("%d/%u == %d/%u\n",
      act->num, act->den, exp->num, exp->den);

    if (act->num != exp->num || act->den != exp->den)
      return ERR_FAILURE;
  }

  return 0;
}

BT_SUITE_DEF(rational, "rational");

BT_TEST_DEF_PLAIN(rational, sr, "sr")
{
  sx_parser_t     parser[1];
  char            t[512];
  int             fd;
  struct stat     sb;
  ssize_t         rd;
  sx_t          * sx;
  err_t           err;
  sx_str_t      * chunk;

  const char * file = "tests/rational-sr-tests.txt";

  if (stat(file, &sb)) {
    printf("could not find %s: %s\n", file, strerror(errno));
    return BT_RESULT_IGNORE;
  }

  fd = open(file, O_RDONLY);
  if (fd == -1) {
    printf("could not open %s: %s\n", file, strerror(errno));
    return BT_RESULT_FAIL;
  }

  bt_assert_ptr_not_equal(sx_parser_init(parser), NULL);

  while ((rd = read(fd, t, 512))) {
    err = sx_parser_read(parser, t, rd);
    if (err) {
      printf("parser error: %s at line %u\n", sx_parser_strerror(parser), parser->line);
      chunk = sx_strgen_get(parser->gen);
      if (chunk) {
        printf("last chunk was: >>%.*s<<\n", chunk->used, chunk->buffer);
        free(chunk);
        goto out;
      }
    }
    bt_assert_int_equal(err, 0);
  }
  //sx_test_print(parser->root, 0, 0, 0);

  sx = parser->root;
  while (sx) {
    bt_chkerr(sr_tester(sx));
    sx = sx->next;
  }

out:
  sx_parser_clear(parser);
  bt_assert_int_equal(err, 0);

/*  bt_chkerr(sr_set(a, 2, 6));
  bt_chkerr(sr_set(b, -1, 2));
  bt_chkerr(sr_add(a, b, c));

  printf("c: %d/%u\n", c->num, c->den);*/

  return BT_RESULT_OK;
}
#endif /* TEST */
