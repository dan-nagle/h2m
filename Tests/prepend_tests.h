#define _bad_macro
#define _bad_func_macro()

int _bad_int;

char* _bad_char_pointer;

double _bad_double_array[15];

char* _bad_char_pointer_array[3][4];

struct _bad_struct {
  int _bad_struct_field;
};

struct _bad_struct _my_bad_struct_var;

int _bad_function(double* _bad_argument);

struct _bad_struct weird_function(struct _bad_struct my_bad, int x);

typedef int _bad_typedef_int;

enum _bad_enum {_bad_enum_member, _bad_enum_member_two};
