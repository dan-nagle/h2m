#include "example_3.h"

void initialize_vars() {
  int i = 0;
  int j = 0;
  for (i =0; i < 4; i++) {
    our_global_numbers[i] = i - 2;
  }

  my_global_char = "MYLITERAL";

  my_global_double = 4.5;
} 
