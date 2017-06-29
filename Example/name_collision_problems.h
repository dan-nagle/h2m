// The following header demonstrates problems with identifiers
// and capitalizations. Though this header will be translated
// to Fortran by h2m, issues will arise during compilation. 
// Often, h2m will detect these problems, warn, and comment
// out the offending line.

// Fortran has no equivalent to C tag names. By using a 
// typedef statement to declare a name for the struct,
// C does not create symbol collisions. However, in 
// Fortran this translates to the declaration of two
// structs with the same name, which would indeed be
// a collision. Here, h2m will detect this and comment
// out the duplicate.
typedef struct my_struct {
  int x;
  double y;
  char z[10];
} my_struct;


// Another problem relates to capitalization. Fortran
// is not case sensistive, but C is. In C, declaring 
// another struct by the name My_Struct is fine, but
// in Fortran it is a name collision. Here, h2m will
// detect this and comment out the duplicate.
struct My_Struct {
  int* x;
  double* y;
  char* z[15];
};
