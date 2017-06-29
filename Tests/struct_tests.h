// Basic tests of h2m's ability to translate
// structured types. This entire file should
// compile without complaint.

struct x {
  int my_int;
  void* my_pointer;
  struct x* my_struct_pointer;
};

struct y {
  double place_holder;
} my_y;

typedef struct y struct_y;

typedef y y;

typedef struct z {
  int* place_holder;
} z;

z my_z;
