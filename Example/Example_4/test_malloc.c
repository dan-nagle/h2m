// This file contains the definition of the 
// very simple shuffling program used to 
// rearrange the array allocated by the 
// Fortran malloc demonstration program.
#include "test_malloc.h"

// Mess up an array in an arbitrary 
// fashion, just to show that changes
// were made.
void shuffle_nums(double my_array[50]) {
  double temp[50];
  int i = 0;
  
  // Randomly mess up our array
  for (i = 0; i < 50; i++) {
    temp[i] = my_array[i];
  }
  for (i = 0; i < 50; i++) {
    if (i < 25) {
      my_array[i] = temp[i*2];
    } else {
      my_array[i] = temp[i/2];
    }
  }
}
