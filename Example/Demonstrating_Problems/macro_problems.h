// Various problems can occur during attempted 
// macro translation. h2m cannot recognize many
// kinds of complicated macros. It will provide
// stub code only to function like macros which
// it does not have enough information to translate
// into fortran equivalents. It cannot recognize
// macros which are defined in terms of other
// macros.

#define my_first_macro 15
// h2m cannot determine what kind of macro
// this is
#define my_second_macro my_first_macro

// h2m cannot make a translation attempt on this
// kind of exotic macro.
#define pthread_like {_CALL_A_FUNCTION, {0}}

// Function-like macros are translated as stubs.
#define swap_trick(x, y) {x ^= y; y ^= x; x ^= y;}

// There is no support for a long long double in Fortran.
#define unallowed_double_long 1000.0LL

// This is not a recognized kind of number
#define unallowed_long_long_long 432LLLu

// Neither is this
#define float_and_long 3420fL
