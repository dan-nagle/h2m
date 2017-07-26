This directory contains a simple demonstration
of how to call a C standard library function 
from Fortran. The gfortran and gcc compilers will
be referenced in example text throughout this 
document, but any reliable compilers should be
fine. Make sure that, if possible, the C and
Fortran compilers used are provided by the same
vendor or organization.
The interoperable program in this example would
like to call the C dynamic memory allocation 
function malloc, fill an allocated array, pass
this array to a C function which will change
the array, and then print the array contents.
The procedure to create this executable is somewhat 
more complex than that for the other examples.

First, find the stdlib.h header file on your system.
Run h2m on the header file. This should look something
like:
../../h2m -o stdlib.f90 /usr/include/stdlib.h

Compile the translated standard library Fortran 
module. This should look something like:
gfortran -c stdlib.f90

Compile the small C file which will be used by the
Fortran program. This should look something like:
gcc -c -o tmc.o test_malloc.c

Run h2m on the header file associated with the small
C file. This should look something like:
../../h2m -o tmc.f90 test_malloc.h

Compile and link the interoperable executable
file. This should look something like this:
gfortran -o test_malloc test_malloc.f90 tmc.o -lc

Note the -lc flags, which inform the linker that
it must link to the C standard libraries, may 
be different on your system.

As a summary, translate and the C
system header file stdlib.h into Fortran then
compile that module, compile the C shuffling 
function, and then compile and link the
interoperable executable, linking to the C 
libraries.


