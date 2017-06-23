#ifndef _EXAMPLE_
#define _EXAMPLE_

// Note: choose to do IO operations either in Fortran or C, 
// not both.
#include <stdio.h>

#define PI 3.14159

struct circle {
  double radius;
  int x;
  int y;
};

double calculate_area(struct circle shape);

void describe_circle(struct circle shape);

#endif  // End example file
