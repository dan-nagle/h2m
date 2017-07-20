// This is a basic test for h2m.
// This should complain about an unrecognized type error.

typedef void my_void_type;

__builtin_va_list my_list[10];

typedef __builtin_va_list va_list;

va_list my_va_list;

int get_stuff(va_list my_list);

void get_stuff2(va_list my_list[]);

struct my_struct {
  va_list illegal;
};
