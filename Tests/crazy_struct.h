typedef struct other {
  long v;
  int t;
  char other_array[2][3];
} other;

double x;
int a_func(int y);

union my_union {
  double y;
  long x;
};

struct thing {
  int x;
  double y;
  char z;
  char m[6];
  struct thing* n;
  const other l; 
  int theirs[3];
  double* double_ptr;
  int (*function_pointer)(int);
};

struct illegal_thing {
  union my_union unicorn;
};

struct thing my_thing = {4, 2.34, 'a', "mine",  0, 
   {100, 10, {{'a', 'b', 'c'}, {'d', 'e', 'f'}}},
   {1, 2, 3}, &x, &a_func};

struct illegal_thing my_illegal_thing = {{5423L}};

union my_union these_unions[2] = {{543L}, {45.6}};


