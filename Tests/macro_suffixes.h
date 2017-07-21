// This file contains macros with a variety of recognized
// and unrecognized suffixes. It should compile without
// complaint.

#define my_long_long 10000000000LL
#define my_u_long_long 10000000000llU
#define my_float 25f
#define my_other_float 90F
#define my_long_float 100000000.0Lu
#define my_illegal_float 1000.0LL
#define my_illegal_float_2 10000.5fL
#define my_int_parens (-5)
#define my_float_parens (-45.4f)
#define my_hex 0xff03bL
#define my_hex_u 0xff02bfu
#define my_oct_u 0234u
#define my_oct_l_l 0424235LL
#define my_bin_l 0b0101L
#define my_bin_u 0b0101u
#define my_bin_u_paren (0b1020u)
#define my_bin_paren (0b1020)
#define illegal_1 343uu
#define illegal_2 432LLLu
#define illegal_3 0452lLuU
#define illegal_4 343.0ff
#define illegal_5 3420fL
