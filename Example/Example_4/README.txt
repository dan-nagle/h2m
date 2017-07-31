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
If problems occur during linking in this example,
a tool such as 'nm' may be useful to inspect 
the exported symbols in an object file. 
Sometimes simply recompiling the component
object files will solve the problems.

First, find the stdlib.h header file on your system.
Run h2m on the header file. This should look something
like:
../../h2m -o stdlib.f90 /usr/include/stdlib.h
You will see a large number of warning messages
from h2m and Clang. None of these should be
relevant to this example.

Compile the translated standard library Fortran 
module. This should look something like:
gfortran -c stdlib.f90

Compile the small C file which will be used by the
Fortran program. This should look something like:
gcc -c -o tmc.o test_malloc.c

Run h2m on the header file associated with the small
C file. This should look something like:
../../h2m -o tmc.f90 test_malloc.h
You will see a warning about illegal names from
h2m. Again, this is not relevant to this 
example.

Compile the translated mini-module. This will
look something like:
gfortran -c tmc.f90

Compile and link the interoperable executable
file. This should look something like this:
gfortran -o test_malloc test_malloc.f90 tmc.o -lc
Note the -lc flags, which inform the linker that
it must link to the C standard libraries, may 
have a different syntax on your system.
If an undefined reference to "shuffle_nums" is
seen, use 'nm' or another tool to check what symbols
are exported by tmc.o. Try recompiling that
object file if you do not see "_shuffle_nums" or
something similar on the list.

Run the executable.
./test_malloc

As a summary, translate and the C
system header file stdlib.h into Fortran then
compile that module, compile the C shuffling 
function, and then compile and link the
interoperable executable, linking to the C 
libraries.

For reference, the output of the example should be:
   1.0000000000000000     
   2.0000000000000000    
   3.0000000000000000     
   4.0000000000000000     
   5.0000000000000000     
   6.0000000000000000     
   7.0000000000000000     
   8.0000000000000000     
   9.0000000000000000     
   10.000000000000000     
   11.000000000000000     
   12.000000000000000     
   13.000000000000000     
   14.000000000000000     
   15.000000000000000     
   16.000000000000000     
   17.000000000000000     
   18.000000000000000     
   19.000000000000000     
   20.000000000000000     
   21.000000000000000     
   22.000000000000000     
   23.000000000000000     
   24.000000000000000     
   25.000000000000000     
   26.000000000000000     
   27.000000000000000     
   28.000000000000000     
   29.000000000000000     
   30.000000000000000     
   31.000000000000000     
   32.000000000000000     
   33.000000000000000     
   34.000000000000000     
   35.000000000000000     
   36.000000000000000     
   37.000000000000000     
   38.000000000000000     
   39.000000000000000     
   40.000000000000000     
   41.000000000000000     
   42.000000000000000     
   43.000000000000000     
   44.000000000000000     
   45.000000000000000     
   46.000000000000000     
   47.000000000000000     
   48.000000000000000     
   49.000000000000000     
   50.000000000000000     
 is the orginal array
   1.0000000000000000     
   3.0000000000000000     
   5.0000000000000000     
   7.0000000000000000     
   9.0000000000000000     
   11.000000000000000     
   13.000000000000000     
   15.000000000000000     
   17.000000000000000     
   19.000000000000000     
   21.000000000000000     
   23.000000000000000     
   25.000000000000000     
   27.000000000000000     
   29.000000000000000     
   31.000000000000000     
   33.000000000000000     
   35.000000000000000     
   37.000000000000000     
   39.000000000000000     
   41.000000000000000     
   43.000000000000000     
   45.000000000000000     
   47.000000000000000     
   49.000000000000000     
   13.000000000000000     
   14.000000000000000     
   14.000000000000000     
   15.000000000000000     
   15.000000000000000     
   16.000000000000000     
   16.000000000000000     
   17.000000000000000     
   17.000000000000000     
   18.000000000000000     
   18.000000000000000     
   19.000000000000000     
   19.000000000000000     
   20.000000000000000     
   20.000000000000000     
   21.000000000000000     
   21.000000000000000     
   22.000000000000000     
   22.000000000000000     
   23.000000000000000     
   23.000000000000000     
   24.000000000000000     
   24.000000000000000     
   25.000000000000000     
   25.000000000000000     
 is the shuffled array

