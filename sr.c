
#include "rational.h"

#define ERR_NULL err_construct(ERR_MAJ_NULL_POINTER, ERR_MIN_IN_NULL_POINTER, 0)
#define ERR_OK err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, 0)

#define ERR_I_OVER err_construct(ERR_MAJ_OVERFLOW, ERR_MIN_IN_OVERFLOW, 0)
#define ERR_C_OVER err_construct(ERR_MAJ_OVERFLOW, ERR_MIN_CALC_OVERFLOW, 0)
#define ERR_DZERO err_construct(ERR_MAJ_INVALID, ERR_MIN_DIVIDE_BY_ZERO, 0)
#define ERR_ err_construct(ERR_MAJ_INVALID, ERR_MIN_DIVIDE_BY_ZERO, 0)
#define ERR_TEST_FAIL err_construct(ERR_TEST_FAILED, ERR_MIN_SUCCESS, 0)

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
    return ERR_NULL;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = num;
  d = den;

  if (d == 0)
    return ERR_DZERO;

  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_C_OVER;
  if (d > UINT16_MAX)
    return ERR_C_OVER;

  rat->num = n;
  rat->den = d;

  return ERR_OK;
}

err_t sr_add(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_NULL;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num*b->den + b->num*a->den;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_C_OVER;
  if (d > UINT16_MAX)
    return ERR_C_OVER;

  result->num = n;
  result->den = d;

  return ERR_OK;
}

err_t sr_sub(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_NULL;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num*b->den - b->num*a->den;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_C_OVER;
  if (d > UINT16_MAX)
    return ERR_C_OVER;

  result->num = n;
  result->den = d;

  return ERR_OK;
}

err_t sr_mul(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_NULL;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num * b->num;
  d = a->den * b->den;
  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_C_OVER;
  if (d > UINT16_MAX)
    return ERR_C_OVER;

  result->num = n;
  result->den = d;

  return ERR_OK;
}

err_t sr_div(const sr_t * a, const sr_t * b, sr_t * result)
{
  if (!a || !b || !result)
    return ERR_NULL;

  signed int n;
  unsigned int d;
  unsigned mul;

  n = a->num * b->den;
  d = a->den * b->num;

  if (d == 0)
    return ERR_DZERO;

  mul = (n>=0)? gcd(n,d) : gcd(-n,d);

  n /= mul;
  d /= mul;

  if (n > INT16_MAX || n < INT16_MIN)
    return ERR_C_OVER;
  if (d > UINT16_MAX)
    return ERR_C_OVER;

  result->num = n;
  result->den = d;

  return ERR_OK;
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
    return ERR_NULL;

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
          return ERR_TEST_FAIL;
        if (sn->next) {
          sd = sn->next;
          if (sd->isuint)
            d = (uint16_t)sd->atom.uint;
          else
            return ERR_TEST_FAIL;
        }
        err = sr_set(ret, n, d);
        if (err.composite)
          return err;
        return ERR_OK;
      } else if (sn->isplain) {
        if (strncmp(sn->atom.string->buffer, "add", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err.composite)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err.composite)
                return err;
              if (err.composite)
                return err;
              err = sr_add(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "sub", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err.composite)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err.composite)
                return err;
              if (err.composite)
                return err;
              err = sr_sub(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "mul", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err.composite)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err.composite)
                return err;
              if (err.composite)
                return err;
              err = sr_mul(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else if (strncmp(sn->atom.string->buffer, "div", sn->atom.string->used) == 0) {
          if (sn->next && sn->next->next) {
            err = sr_test_eval(sn->next, a);
            if (err.composite)
              return err;
            sn = sn->next->next;
            while (sn) {
              err = sr_test_eval(sn, b);
              if (err.composite)
                return err;
              if (err.composite)
                return err;
              err = sr_div(a, b, a);
              sn = sn->next;
            }
            *ret = *a;
          }
        } else
            return ERR_TEST_FAIL;
      }
    }
  }

  return ERR_OK;
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
      return ERR_OK;

    cal = sx;
    if (!cal)
      return ERR_OK;
    res = cal->next;
    if (!res)
      return ERR_OK;

    err = sr_test_eval(cal, act);
    if (err.composite)
      return err;
    err = sr_test_eval(res, exp);
    if (err.composite)
      return err;

    bt_log("(");
    sx_test_print(cal, 0, 0, 0, 0);
    bt_log(") == (");
    sx_test_print(res, 0, 0, 0, 0);
    bt_log(") <=> ");
    bt_log("%d/%u == %d/%u\n",
      act->num, act->den, exp->num, exp->den);

    if (act->num != exp->num || act->den != exp->den)
      return ERR_TEST_FAIL;
  }

  return ERR_OK;
}

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
    bt_log("could not find %s: %s\n", file, strerror(errno));
    return BT_RESULT_IGNORE;
  }

  fd = open(file, O_RDONLY);
  if (fd == -1) {
    bt_log("could not open %s: %s\n", file, strerror(errno));
    return BT_RESULT_FAIL;
  }

  bt_assert_ptr_not_equal(sx_parser_init(parser), NULL);

  while ((rd = read(fd, t, 512))) {
    err = sx_parser_read(parser, t, rd);
    if (err.composite) {
      bt_log("parser error: %s at line %u\n", sx_parser_strerror(parser), parser->line);
      chunk = sx_strgen_get(parser->gen);
      if (chunk) {
        bt_log("last chunk was: >>%.*s<<\n", chunk->used, chunk->buffer);
        sx_pool_retmem(parser->pool, chunk);
        goto out;
      }
    }
    bt_assert_int_equal(err.composite, 0);
  }
  //sx_test_print(parser->root, 0, 0, 0);

  sx = parser->root;
  while (sx) {
    bt_chkerr(sr_tester(sx));
    sx = sx->next;
  }

out:
  sx_parser_clear(parser);
  bt_assert_int_equal(err.composite, 0);

/*  bt_chkerr(sr_set(a, 2, 6));
  bt_chkerr(sr_set(b, -1, 2));
  bt_chkerr(sr_add(a, b, c));

  bt_log("c: %d/%u\n", c->num, c->den);*/

  return BT_RESULT_OK;
}
#endif /* TEST */