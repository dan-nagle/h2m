// This is a basic test of macro translation functionality.
// All should compile and translate without issue.

#define my_integer 45

#define my_double 34.5

#define my_hex 0x345

#define my_long 123L

#define my_char 'a'

#define my_string "a string"

#define my_arg_macro(x, y) x + y

#define swap_trick(x, y) {x ^= y; y ^= x; x ^= y;}

#define multi_line(x) {  \
    x++;  \
    x--;  \
}
