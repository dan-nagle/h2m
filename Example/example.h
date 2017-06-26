#ifndef _EXAMPLE_
#define _EXAMPLE_

// Note: choose to do IO operations either in Fortran or C, 
// not both.
#include <stdio.h>

// Define PI as a constant.
#define PI 3.14159

// Create a structure representing a circle.
struct circle {
  double radius;
  int x;
  int y;
};

// Declare two function acting on the circle struct declared.
double calculate_area(struct circle shape);

void describe_circle(struct circle shape);

#endif  // End example file
