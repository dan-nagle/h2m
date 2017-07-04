// This is a basic test for array functionality
// in h2m. It should compile without complaint.

int my_int_array[10];

double* my_double_array[200];

char my_char_array[2] = {'a', 'b'};

char my_literal_char[] = "Literal string";

// This causes a problem because it is not 
// initialzied and Clang doesn't seem to 
// provide the ability to determine the
// dimension.
int my_2d_array[4][4];

int my_other_2d_array[1][2] = {'a', 'b'};

int my_array[];

