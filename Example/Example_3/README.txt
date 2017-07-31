This folder contains a contrived example which demonstrates
how to use C global variables interoperably with Fortran.
Note that all Fortran variables must have the BIND(C) attribute in 
order to be interoperable with C.

To run this example, follow the pattern demonstrated in the
Example_1 folder. First, translate the header file into Fortran
using h2m (ie ../../h2m -o ex3_mod.f90 -b example_3.h). The -b
option requests that the renamed Fortran variable be linked to
the originally named C variable. This option is available for global
variables and functions. It is not available for structs.
Compile but do not link the C file (ie gcc -c -o ex3.o example_3.c).
Compile but do not link the module (ie gfortran -c ex3_mod.f90).
Compile the entire program (ie gfortran example_3.f90 ex3.o).
Run the executable: ./a.out. 

The output should be: 

 Our character is: 
 MYLITERAL
 Our global double is: 
   4.5000000000000000     
 Our _my_global_other/h2m_my_global_other is: 
           0
 Our array is: 
          -2
          -2
          -2
          -2
          -1
           0
          -2
           0
           2
          -2
           1
           4

