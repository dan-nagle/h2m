// This is a basic test of macro translation functionality.
// All should compile and translate without issue.

#define my_integer 45

#define my_double 34.5

#define my_binary 0b10101101
#define my_binary_caps 0B000
#define my_not_binary 0B20

#define my_octal 05234
#define not_octal 08323
#define my_other_not_octal 0L832

#define my_other_int 345L

#define my_unsigned_int 45UL

#define my_hex 0x345
#define my_other_hex 0Xfedab
#define my_not_hex 0xfg01

#define my_long 123L

#define my_char 'a'

#define my_string "a string"

#define my_arg_macro(x, y) x + y

#define swap_trick(x, y) {x ^= y; y ^= x; x ^= y;}

#define multi_line(x) {  \
    x++;  \
    x--;  \
}
