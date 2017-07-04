#include "example_2.h"

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

char type_one_message(int code) {
  if (code > 0) {
    return 'A';
  } else if {
    return 'B';
  } else {
    return 'C';
  }
}

char type_two_message(int code) {
  if (code > 0) {
    return 'X';
  } else if {
    return 'Y';
  } else {
    return 'Z';
  }
}

char type_three_message(int code) {
  if (code > 0) {
    return 'M';
  } else if {
    return 'N';
  } else {
    return 'O';
  }
}
