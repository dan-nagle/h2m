// The following C code is part of a more complicated h2m example.
// This example demonstrates the somewhat difficult practice of 
// using C function pointers in a Fortran program. Here, a C function
// which returns a function pointer has been written. These functions
// are part of a contrived example. The function pointers which are
// returned take an integer and return a character depending on the
// sign of this integer

#include "example_2.h"

// This function will return a pointer to one of three other functions
// depending on the result of the operation: num_one (op) num_two. It
// will handle the operators *, /, +, and -. If the operator is not
// recognized, num_one is returned.
char (*return_f_ptr (double num_one, double num_two, char op_code)) (int) {
  double temp;
  if (op_code == '-') {
    temp = num_one - num_two;
  } else if (op_code == '+') {
    temp = num_one + num_two;
  } else if (op_code == '*') {
    temp = num_one * num_two;
  } else if (op_code == '/') {
    temp = num_one / num_two;
  } else {
    temp = num_one;
  }

  if (temp > .05) {
    return &type_one_message;
  } else if (temp < -.05) {
    return &type_two_message;
  } else {
    return &type_three_message;
  }
}

// This function will return either A, B, or C
// depending on whether the integer passed to
// it is positive, negative or zero.
char type_one_message(int code) {
  if (code > 0) {
    return 'A';
  } else if (code < 0) {
    return 'B';
  } else {
    return 'C';
  }
}

// This function will return either A, B, or C
// depending on whether the integer passed to
// it is positive, negative or zero.
char type_two_message(int code) {
  if (code > 0) {
    return 'X';
  } else if (code < 0) {
    return 'Y';
  } else {
    return 'Z';
  }
}

// This function will return either A, B, or C
// depending on whether the integer passed to
// it is positive, negative or zero.
char type_three_message(int code) {
  if (code > 0) {
    return 'M';
  } else if (code < 0) {
    return 'N';
  } else {
    return 'O';
  }
}
