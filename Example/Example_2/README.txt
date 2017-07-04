This folder contains a contrived example which demonstrates
how to use C function pointers in Fortran. Note that calling
a C function pointer requires the use of a special conversion
function included in the intrinsic iso_c_binding module. 
Note that function pointers and abstract interfaces are part
of the Fortran 2003 standard. Name files accordingly.

To run this example, follow the pattern demonstrated in the
Example_1 folder. First, translate the header file into Fortran
using h2m (ie ../../h2m -o ex2_mod.f03 example_2.h).
Compile but do not link the C file (ie gcc -c -o ex2.o example_2.h).
Compile but do not link the module (ie gfortran -c ex2_mod).
Compile the entire program (ie gfortran example_2f.f03 ex2.o).
Run the executable: ./a.out. 


