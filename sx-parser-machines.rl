
%%{
  machine sx_parser_estring_machines;

  # "http://www.unicode.org/versions/Unicode5.0.0/ch03.pdf#G7404"
  utf8_char = (
                (ascii)
                |  (0xC2..0xDF  0x80..0xBF)
                |  (0xE0        0xA0..0xBF  0x80..0xBF)
                |  (0xE1..0xEC  0x80..0xBF  0x80..0xBF)
                |  (0xED        0x80..0x9F  0x80..0xBF)
                |  (0xEE..0xEF  0x80..0xBF  0x80..0xBF)
                |  (0xF0        0x90..0xBF  0x80..0xBF  0x80..0xBF)
                |  (0xF1..0xF3  0x80..0xBF  0x80..0xBF  0x80..0xBF)
                |  (0xF4        0x80..0x8F  0x80..0xBF  0x80..0xBF)
              ) ;

  string_char = utf8_char -- (0x00..0x1F | 0x7f);

  odigit = ('0'..'7') ;

  escape =  ("\\"(([0abfnrtv]) @a_escape_char)) 
            | ((
              ('\\o' (('0'..'3') @a_escape_oct) (odigit @a_escape_oct){2}) @a_escape_int
              | ('\\x' (xdigit @a_escape_hex){2}) @a_escape_int
              | ('\\u' (xdigit @a_escape_hex){4}) @a_escape_uni
            ) >a_escape_clear);

  sq_string_literal =
    (
      "'"
        (
          (((string_char >a_string_char_in @a_string_char_out)+ -- ('\\') | escape)+ -- ("'"|"\\'"))
          | ("\\'" @a_escape_char)
          | ("\\\\" @a_escape_char)
        )*
      "'"
    ) ;

  dq_string_literal =
    (
      '"'
        (
          (((string_char >a_string_char_in @a_string_char_out)+ -- ('\\') | escape)+ -- ('"'|'\\"'))
          | ("\\'" @a_escape_char)
          | ("\\\\" @a_escape_char)
        )*
      '"'
    ) ;

  machine sx_parser_machines;

  odigit = ('0'..'7');
  bdigit = ('0'..'1');

  # this was >> iden = utf8_char -- ([\t\n\r\v ]); << but did not make too mauch sense...
  iden = alnum|[_:@\.];

  int = 
      ((('-' @a_int_sign | '+' @a_int_psign)? (
          ('0y' (bdigit @a_int_bin)+)
        | ('0o' (odigit @a_int_oct)+)
        | (     (digit  @a_int_dec)+)
        | ('0x' (xdigit @a_int_hex)+)
      )) >a_int_in @a_int_out );

  plain = ((iden - ([\-+]|digit)) iden*) >a_plain_in;

}%%

// vim: filetype=c:syn=ragel
