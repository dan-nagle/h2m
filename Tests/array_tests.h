// This is a basic test for array functionality
// in h2m. It should compile without complaint.

#define array_const  10

struct simple {
  int x;
};

int my_int_array[10];

double* my_double_array[200];

char my_char_array[2] = {'a', 'b'};

char my_other_char_array[2][3] =  	{ 
  {'a', 'b', 'c'},
  {'c', 'd', 'e'}
};

char my_literal_char[] = "Literal string";

int my_2d_array[4][4];

int my_other_2d_array[1][2] = {'a', 'b'};

double my_2d_double_array[2][2] = {{2.5, -1.3}, {0, 5.3}};

double my_3d_array[3][5][8];

struct simple struct_array[5][2];

int my_array[];

int my_var_array[array_const];

float my_float_array[array_const+5][array_const+1][array_const+2];

int my_variable_function(int my_array[*], double other[]);

int my_other_function(int my_array[4][5], int x, double other[x+4][x*2-3]);

int other_function(int my_array[array_const], double other[array_const][array_const*2]);

int static_array_func(int my_array[static 5]);

