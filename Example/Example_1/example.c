#include "example.h"

// Use the common forumlate pi*r^2 to calculate
// the area of a cirle struct.
double calculate_area(struct circle shape) {
  double area;
  area = PI * shape.radius * shape.radius;
  return area;
}

// Print a short description of the circle structure.
void describe_circle(struct circle shape) {
  printf("My circle is at x,y %d,%d and has radius %f.\n",
      shape.x, shape.y, shape.radius);
}
