// The following C header file is used to declare the functions that
// the Fortran program might like to use. This is a contrived example
// to demonstrate the use of C function pointers in a Fortran program.

char (*return_f_ptr (double num_one, double num_two, char op_code)) (int);

char type_one_message(int code);

char type_two_message(int code);

char type_three_message(int code);
