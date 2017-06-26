#!/bin/sh
# This script should run under bash, ksh, and zsh. Other
# shells may also work but no guarantees can be made.
# This script is designed to give a very brief introduction
# to the usage of h2m and the general process of calling
# C routines from Fortran.


# Reports the error condition and exits
error_report ()
{
  echo "Error. $1">&2
  exit 1
}

# Check to make sure the files we expect to find are present.
if [ ! -f "$PWD"/example.c ] || [ ! -f "$PWD"/example.h ] || [ ! -f "$PWD"/example.f90 ]
then
  error_report "Please run the tutorial script from the Example directory."
fi
# Initial values are empty. If they are still empty after auto-detect, we have a problem
fortran_compile_command="none"
fortran_compiler_driver="none"
c_compile_command="none"

if [ `which gfortran` ]  # We have found our preferred compiler, gfortran
then
  fortran_compile_command="gfortran -c"
  fortran_compiler_driver="gfortran -o" 
elif [ `which ifort` ]  # We have found the intel fortran compiler
then
  fortran_compile_command="ifort -c -o"
  fortran_compiler_driver="ifort -o"
fi
# We have searched for the fortran compiler and now look for C.
if [ `which gcc` ]  # We have found our preferred compiler, gcc
then
  c_compile_command="gcc -c -I. -o"
elif [ `which icc` ]  # We have found the intel C compiler
then
  c_compile_command="icc -c -I. -o"
elif [ `which clang` ]  # We have found the llvm C compiler
then 
  c_compile_command="clang -c -I. -o"
fi

# If we have failed to find adequate compilers, beg for the user's help
if [ "$fortran_compile_command" == "none" ] || [ "$fortran_compiler_driver" == "none" ]
then
  echo "Autodetection of a Fortran compiler has failed. To continue, please"
  echo "provide the following information about your Fortran compiler."
  echo "An empty response will exit the tutorial."
  echo "What is the command to invoke your Fortran compiler?"
  read compiler_name
  if [ "$compiler_name" == "" ]  # No response was given. We cannot continue.
  then
    error_report "No compiler provided. Exiting."
  else 
    echo "Proceeding with compiler $compiler_name"
  fi
  echo "What is the option to compile but not link Fortran files with your compiler?"
  echo "The default, used if none is provided, is -c. This is the case for gfortran and ifort."
  read compile_only_option
  if [ "$compile_only_option" == "" ]  # No response was given. Use the default -c option.
  then
    compile_only_option="-c"
  fi
  echo "What is the option to specify an output file with your compiler?"
  echo "The default, used if none is provided, is -o. This is the case for gfortran and ifort." 
  read output_option
  if [ "$output_option" == "" ]  # No response was given. Use the default -o option
  then
    output_option="-o"
  fi

  # Sort the information into the proper commands
  fortran_compile_command="$compiler_name $compile_only_option"
  fortran_compiler_driver="$compiler_name $output_option"
fi  # End interactive begging for fortran compiler information

# If we have failed to find adequate C compilers, beg for the user's help
if [ "$c_compile_command" == "none" ]
then
  echo "Autodetection of a C compiler has failed. To continue, please"
  echo "provide the following information about your C compiler."
  echo "An empty response will exit the tutorial."
  echo "What is the name of your C compiler?"
  read c_compiler_name
  if [ "$c_compiler_name" == "" ]  # No response was given. We cannot continue.
  then
    error_report "No compiler provided. Exiting."
  fi
  echo "What is the option to compile but not link C files with your compiler?"
  echo "The default, used if none is provided, is -c. This is the case for icc, clang, and gcc."
  read c_compile_only_option
  if [ "$c_compile_only_option" == "" ]  # No response was given. Use the default -c option.
  then
    c_compile_only_option="-c"
  fi
  echo "What is the option to include the current directory while searching for header"
  echo "files with your compiler? The default, used if none is provided, is -I. This is"
  echo "the case for icc, clang, and gcc."
  read c_include_option
  if [ "$c_include_option" == "" ]
  then
    c_include_option="-I." 
  fi
  echo "What is the option to specify the output file with your compiler?"
  echo "The default, if none is provided, is -o. This is the case for icc, clang, and gcc."
  read c_output_option
  if [ "$c_output_option" == "" ]
  then
    c_output_option="-o"
  fi
 
  # Assemble the C command we need
  c_compile_command="$c_compiler_name $c_include_option $c_compile_only_option $c_output_option"     
  
fi

# We have adequate compiler information. On to the fun!
echo "Welcome to a very brief h2m tutorial."
echo "Press enter to continue."
read continue   # Method of pausing execution.

# Explain what the files are and show off their content
echo "Here we have a C source file, a C header file, and a Fortran source file."
ls example.h example.c example.f90  # No real need for error checking here. This wouldn't be a problem if it failed.
echo "The header file declares a structure, a macro, and two functions."
echo ""  # Portably skip a line
cat example.h # No real need for error checking here, either. 
echo ""  # Portably skip another line
echo "Press enter to continue."
read continue
echo "The C file defines the functions declared in the header file."
echo ""
cat example.c
echo "Press enter to continue."
echo ""
read continue
echo "Note the use of the intrinsic iso_c_binding module which defines kinds such as"
echo "C_DOUBLE to allow interoperability."
echo "The Fortran file would like to create a 'circle' structure defined in the C header."
echo "It would also like to call functions defined and implemented in the C files."
echo ""
cat example.f90
echo ""
echo "Press enter to continue."
read continue
echo "First, the Fortran program needs an interface to the C functions."
echo "The header file provides an interface written in C, and h2m can"
echo "create a Fortran interface from this C header file."

if [ `which h2m` ]  # Attempt to find h2m either in the above directory or installed
then
  h2m_command="h2m"
elif [ -f ../h2m ]
then
  h2m_command="../h2m"
else  # We didn't find it. Beg for help
  echo "Autodetection of h2m has not succeeded. To continue, please provide"
  echo "the command necessary to launch the h2m executable (ie ../../h2m)"
  echo "Provide an empty response to exit."
  read h2m_command
  if [ "$h2m_command" == "" ]  # The user gave an empty response. Exit.
  then
    error_report "Exiting due to lack of provided h2m executable information"
  fi
fi

# Run h2m after a brief explanation 
echo "To create a module file of the example.h, the following command is run:"
echo "$h2m_command -out=translated_header.f90 example.h"
echo "For more detailed information about the usage of h2m, refer to the README.txt"
echo "file or run $h2m_command -help or $h2m_command -help-hidden"
echo "The h2m program will now translate the header file."
echo "Press enter to continue."
echo ""
read continue
`echo "$h2m_command -out=translated_header.f90 example.h"` || error_report "Unfortunately, the h2m translation failed." 
echo "Note the warning provided. Because Fortran names cannot begin with an underscore,"
echo "the prefix 'h2m' was prepended to the headerguard macro _EXAMPLE_ to make a legal"
echo "Fortran identifier."
echo "Note that when h2m changes the names of functions or structs in this way, the"
echo "C function or struct's name must be changed to allow interoperatiblity."
echo "For example, if there is a function _calculate in C, which h2m translates to"
echo "h2m_calculate in Fortran, the processor will have no idea they are supposed to refer to"
echo "the same function."
echo "The translated module provides the necessary interface to use the C"
echo "structure and call the C functions."
echo "Press enter to view the translated file."
read continue
cat translated_header.f90

# Compile the needed files into object files.
echo ""
echo "The needed interface has been written. Now the program must be compiled and linked."
echo "We compile the C file into the object file example_c.o as:"
echo "$c_compile_command example_c.o"  # The -o option is the last part of the compile command
echo "And we compile the translated Fortran module into a .mod file, module_example.f90, as:"
echo "$fortran_compile_command translated_header.f90"
echo "Press enter to compile"
read continue
`echo "$c_compile_command example_c.o example.c"` || error_report "Unfortunately, the C file compilation failed."
`echo "$fortran_compile_command translated_header.f90"` || error_report "Unfortunately, the module file compilation failed."

# Compile the entire suite of files together
echo ""
echo "Now that we have the needed pieces, we can compile the entire suite."
echo "The command to compile the main Fortran program and link it to the C program is:"
# Again, the output specification is the last part of the command
echo "$fortran_compiler_driver example example.f90 example_c.o"  # The module file isn't a needed argument. It's found automatically.
echo "Press enter to compile"
read continue
`echo "$fortran_compiler_driver example example.f90 example_c.o"` || error_report "Unfortunately, compilation failed."

# Run the executable
echo ""
echo "Now we can run the executable and see the results."
echo "Press enter to run the executable."
read continue
./example || error_report "Unfortunately, the executable did not run correctly."
echo "This concludes the very brief h2m tutorial."
echo "In summary, translate your header file with h2m, compile your C file,"
echo "compile your module file, and then compile the main Fortran executable"
echo "and link it with your C object file."

