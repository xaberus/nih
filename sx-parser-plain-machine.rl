
#include <stdio.h>
#include <string.h>

#include "sx.h"




%%{
  machine sx_plain;

  alphtype unsigned char;

  action a_int_bin {
    const uint64_t base = 2;
    const uint64_t cutoff = UINT64_MAX / base;
    const uint64_t cutoff_digit = UINT64_MAX - cutoff * base;

    unsigned int digit = (*p-'0');
    if ((u > cutoff) || ((u == cutoff) && (digit > cutoff_digit))) {
      sx->issint = sx->isuint = 0;
      sx->isplain = 1;
      fnext *0;
    }

    u = u * base + digit;
  }
  action a_int_oct {
    const uint64_t base = 8;
    const uint64_t cutoff = UINT64_MAX / base;
    const uint64_t cutoff_digit = UINT64_MAX - cutoff * base;

    unsigned int digit = (*p-'0');
    if ((u > cutoff) || ((u == cutoff) && (digit > cutoff_digit))) {
      sx->issint = sx->isuint = 0;
      sx->isplain = 1;
      fnext *0;
    }

    u = u * base + digit;
  }
  action a_int_dec {
    const uint64_t base = 10;
    const uint64_t cutoff = UINT64_MAX / base;
    const uint64_t cutoff_digit = UINT64_MAX - cutoff * base;

    unsigned int digit = (*p-'0');
    if ((u > cutoff) || ((u == cutoff) && (digit > cutoff_digit))) {
      sx->issint = sx->isuint = 0;
      sx->isplain = 1;
      fnext *0;
    }

    u = u * base + digit;
  }
  action a_int_hex {
    const uint64_t base = 16;
    const uint64_t cutoff = UINT64_MAX / base;
    const uint64_t cutoff_digit = UINT64_MAX - cutoff * base;

    unsigned int digit = 20;

    if (*p >= '0' && *p <= '9')
      digit = (*p-'0');
    else if (*p >= 'a' && *p <= 'f')
      digit = (*p-'a' + 10);
    else if (*p >= 'A' && *p <= 'F')
      digit = (*p-'A' + 10);

    if (digit > base)
      fnext *0;
    if ((u > cutoff) || ((u == cutoff) && (digit > cutoff_digit))) {
      sx->issint = sx->isuint = 0;
      sx->isplain = 1;
      fnext *11;
    }

    u = u * base + digit;

  }
  action a_int_sign {
    sign = 1;
    sx->isuint = 0;
    sx->issint = 1;
  }
  action a_int_psign {
    sign = 0;
    sx->isuint = 0;
    sx->issint = 1;
  }
  action a_int_in {
    u = 0;
    sign = 0;
    sx->isuint = 1;
  }
  action a_int_out {
    /*bt_log("» %c%x = %c%d\n", 
      sign ? '-': '+', u,
      sign ? '-': '+', u);*/
  }
  action a_plain_in {
    sx->isplain = 1;
  }
  action a_atom {
    if (sx->isuint)
      sx->atom.uint = u;
    else if (sx->issint)
      if (!sign && u <= INT64_MAX)
        sx->atom.sint = (int64_t)u;
      else if (sign && (u <= 0x7ffffffffffffffe))
        sx->atom.sint = (int64_t)(-u);
      else {
        sx->issint = sx->isuint = 0;
        sx->isplain = 1;
        sx->atom.string = str;
      }
    else if (sx->isplain)
      sx->atom.string = str;
  }

  include sx_parser_machines "sx-parser-machines.rl";

  sx_plain := (int | plain) %a_atom;

}%%

%% write exports;
/**/
%% write data;

err_t sx_parser_atom_plain_fsm(sx_parser_t * parser, sx_t * sx, sx_str_t * str)
{
  int cs;
  unsigned char * p = (unsigned char *)str->buffer;
  unsigned char * pe = p + str->used;
  unsigned char * eof = pe;

  uint64_t u;
  int sign;

  %% write init;

  /*bt_log("» <%.*s> (%u)\n", (int)(pe-p), p, (int)(pe-p));*/

  %% write exec;

  if ( cs < sx_plain_first_final) {
    /*bt_log("rejected at <%.*s> in %d\n", (int) (pe-p+1), p-1, cs);*/
    parser->err = err_construct(ERR_MAJ_INVALID, ERR_MIN_IN_INVALID, SX_ERROR_UNHANDLED);
    sx_pool_retmem(parser->pool, str);
  } else {
    /*bt_log("« <%.*s> (%u)\n", str->used, str->buffer, str->used);*/
    parser->err = err_construct(ERR_MAJ_SUCCESS, ERR_MIN_SUCCESS, SX_ERROR_SUCCESS);
    if (!sx->isplain)
      sx_pool_retmem(parser->pool, str);
  }

  return parser->err;
}

// vim: filetype=c:syn=ragel
