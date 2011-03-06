
#include <stdio.h>
#include <string.h>

#include "sx.h"

%%{
  machine sx_sq;

  alphtype unsigned char;

  action a_escape_char {
    /*bt_log("escape <%c>\n", *p);*/
    switch (*p) {
      case '0':
        *w = '\0'; w++; break;
      case 'a':
        *w = '\a'; w++; break;
      case 'b':
        *w = '\b'; w++; break;
      case 'f':
        *w = '\f'; w++; break;
      case 'n':
        *w = '\n'; w++; break;
      case 'r':
        *w = '\r'; w++; break;
      case 't':
        *w = '\t'; w++; break;
      case 'v':
        *w = '\v'; w++; break;
      default:
        *w = (unsigned char)*p; w++;
    }
  }
  action a_escape_clear {
    u = 0;
  }
  action a_escape_oct {
    u = (u << 3) | (*p-'0');
  }
  action a_escape_hex {
    if (*p >= '0' && *p <= '9')
      u = (u << 4) | (*p-'0');
    else if (*p >= 'a' && *p <= 'f')
      u = (u << 4) | (*p-'a' + 10);
    else if (*p >= 'A' && *p <= 'F')
      u = (u << 4) | (*p-'A' + 10);
  }
  action a_escape_int {
    /*bt_log("escape int <%u = %c = 0x%04hx>\n", u, u, u);*/
    *w = (unsigned char)u; w++;
  }
  action a_escape_uni {
    unsigned char buff[6] = {0};
    unsigned int blen;
    blen = sx_parser_encode_utf8(u, buff);
    /*bt_log("escape char 0x%08x -> <%.*s>\n", u, blen, buff);*/
    for (unsigned int i = 0; i < blen; i++, w++)
      *w = buff[i];
  }
  action a_string_char_in {
    s = p;
  }
  action a_string_char_out {
    /*bt_log("char <%.*s>\n", (int) (p+1-s), s);*/
    for (unsigned int i = 0, len = p+1-s; i < len; i++, w++, s++)
      *w = *s;
  }

  include sx_parser_estring_machines "sx-parser-machines.rl";

  sx_sq := sq_string_literal;

}%%

%% write exports;
/**/
%% write data;

err_t sx_parser_atom_sq_fsm(sx_parser_t * parser, sx_str_t * str)
{
  int cs;
  unsigned char * p = (unsigned char *)str->buffer;
  unsigned char * pe = p + str->used;
  unsigned char * s;
  unsigned char * w = p;

  uint32_t u;

  %% write init;

  /*bt_log("» <%.*s> (%u)\n", (int)(pe-p), p, (int)(pe-p));*/

  %% write exec;

  if ( cs < sx_sq_first_final) {
    /*bt_log("rejected at <%.*s>\n", (int) (pe-p+1), p-1);*/
    parser->err = ERR_SX_UNHANDLED;
  } else {
    str->used = ((char *) w - str->buffer);
    /*bt_log("« <%.*s> (%u)\n", str->used, str->buffer, str->used);*/
    parser->err = 0;
  }

  return parser->err;
}
