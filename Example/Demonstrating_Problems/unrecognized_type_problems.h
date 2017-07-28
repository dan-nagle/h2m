// The following header demonstrates problems with unrecognized
// type names which can arise during translation. This header
// will not be translated properly by h2m because it will be
// unable to recognize the type names used. It will warn
// about the errors and surround the unrecognized type in 
// brackets. The translated Fortran will, of course, not
// compile.


// Fortran has no equivalent for a void type, thus h2m
// will not recognize or translate it.
typedef void my_alias_for_void;

// Very complicated types, particulary in typedef statements,
// can also cause these problems, but they are difficult to
// replicate in a controlled environment. Another villain
// sometimes encountered is __builtin_va_list. This problem
// is so common in system headers that h2m specifically
// checks for type names containing va_list and comments 
// them out.

__builtin_va_list mylist[10];

struct my_struct {
  __builtin_va_list illegal;
};
