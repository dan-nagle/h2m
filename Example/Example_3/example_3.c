// This is part of a contrived example to demonstrate
// the use of interoperable global constants. The constants
// are declared in the header, initialized in a void function
// here, and printed later by the Fortran program.

#include "example_3.h"
#include <string.h>

// Initialize all the variables declared in the header.
void initialize_vars() {
  int i = 0;
  int j = 0;
  for (i = 0; i < 4; i++) {
    for (j = 0; j < 3; j++) {
      our_global_numbers[i][j] = i * j - 2;
    }
  }

  // Note that initializing this as a string literal, ie
  // my_global_char = "MYLITERAL" will not work. Fortran
  // does not properly access C static or dynamic memory.
  strncpy(my_global_char, "MYLITERAL", 9);

  my_global_double = 4.5;
  _my_global_other = 0;
} 
