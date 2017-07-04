This folder contains a contrived example which demonstrates
how to use C global variables interoperably with Fortran.
Note that all Fortran variables must have the BIND(C) attribute in 
order to be interoperable with C.

To run this example, follow the pattern demonstrated in the
Example_1 folder. First, translate the header file into Fortran
using h2m (ie ../../h2m -o ex3_mod.f90 example_3.h).
Compile but do not link the C file (ie gcc -c -o ex3.o example_3.c).
Compile but do not link the module (ie gfortran -c ex3_mod).
Compile the entire program (ie gfortran example_3.f90 ex3.o).
Run the executable: ./a.out. 


