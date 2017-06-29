// This is a basic test of the ability of h2m
// to translate functions. All of these should
// compile without complaint.

struct simple {
  int a;
};

int my_simple_function();

void my_simple_subroutine();

double my_double_function(int x, double* y, char[] z);

void my_complex_subroutine(short v, long** c, struct simple* m);

struct simple return_struct();

struct simple* return_struct_pointer();

void (*) int return_function_pointer();
