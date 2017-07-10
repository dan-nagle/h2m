#include <stdio.h>
// The following header demonstrates problems with lengths
// of lines and names which can arise during translation. 
// Though this header will be translated to Fortran by h2m,
// issues will arise during compilation. Often, h2m will
// detect these problems, warn, and comment out the 
// offending line.

// C has no limit to line lengths, but Fortran does. 
// Occasionally, particularly during typedefs or 
// function declaratoins, lines which are too long
// for Fortran (greater than 132 characters) will
// be created. This can be fixed manually by breaking
// the statement over several lines using the ampersand.
void too_long_for_fortran(char* long_parameter_one, int long_parameter_two,
     int long_parameter_three[50], double long_parameter_four, 
     double long_parameter_five, int long_parameter_six) {
   printf("This function's declaration is too long for Fortran.");
} 

// C does not limit the length of an identifier's name, but a
// Fortran identifier may not be longer than 63 characters
// according to the Fortran 2013 standard.
int my_ridiculous_integer_has_an_unnecessary_and_extremely_long_identifier;

struct my_very_long_type_name_is_problematic {
  int placeholder;
};
// This typedef is very problamatic because, in h2m's 
// approximate translation of typedefs, the parameter's 
// name will become far too long.
typedef struct my_very_long_type_name_is_problematic a_problematic_structure_name_length;

