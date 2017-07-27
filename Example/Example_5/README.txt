This example is presented less for the purpose
of demonstrating how to compile and link
mixed-language executables, but more for the
purpose of demonstrating how to write more
complicated Fortran programs calling more
complicated C functions. These can be used
for reference.

If the netcdf library actually is installed,
the example in this directory can probably
be run. Complications may ensue.
In order to actually compile this executable
and link to the netcdf library, the following
approximate steps should be followed. These
will be dependent on the system used.
Though icc and ifort will be used as
compilers in the example text in this
document, any reliable compilers should work.
Make sure the C and Fortran compiler are
provided by the same organization or vendor
if possible.

First, locate the your system's netcdf.h header
file. Run h2m on this file. This should look
something like:
h2m -o netcdf.f90 /path/to/netcdf/netcdf.h

Compile the translated Fortran module file.
This should look something like:
ifort -c netcdf.f90

Compile and link the two executables. The 
netcdf and hdf5 linker flags may be system dependent.
This should look something like:
ifort -o writeprog ncdf_write.f90 -lnetcdf -lhdf5
ifort -o readprog ncdf_read.f90 -lnetcdf -lhdf5

These executables should write to and read from
a netcdf file. Remember to run the write 
executable first.
