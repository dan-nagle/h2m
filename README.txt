                          h2m Autofortran Tool Readme
______________________________________________________________________

Contents:
  0) Quick Start
  1) Introduction and Credits
  2) Build and Installation
     General informatoin
     Usage of the h2mbuild.sh script
     Troubleshooting
  3) Usage of h2m
  4) Known Issues

0)   Quick Start
Always run ./h2mbuild.sh. Do not run it from a different working 
directory. 

To build h2m if you do not have Clang or LLVM installed, run:
./h2mbuild.sh -download -install_dir [path to desired Clang/LLVM download]
or run ./h2mbuild.sh -i for interactive mode.

To build h2m if you have Clang and LLVM installed, run:
./h2mbuild.sh -CLANG_DIR [absolute path to ClangConfig.cmake]
-LLVM_DIR [absolute path to LLVMConfig.cmake]
or run ./h2mbuild.sh -i for interactive mode.
If Clang and LLVM are installed in standard places, these directory
specifications may not be necessary.
If you cannot find the Config.cmake files, see Build and Installation.
To run h2m after the build, run:

./h2m [path to C header file] 

1)   INTRODUCTION AND CREDITS

The h2m Autofortran tool is designed to allow easy calls to C
routines from Fortran programs. Given a header file in standard C,
h2m will produce a Fortran module providing function interfaces
which maintain interoperability with C. Features for which there
are no Fortran equivalents will not be translated. 
The h2m Autofortran tool is build into Clang, the LLVM C compiler.
During translation, the Clang abstract syntax tree is used to 
assemble information about the header file. Clang is a very strict
compiler and will often generate warnings about the C code during
this phase.

The h2m Autofortran tool was envisioned by Dan Nagle and completed
at NCAR by Garnet **I DON'T KNOW HER LAST NAME** and revised by
Michelle Anderson and *** VARIOUS ACKNOWLEDGMENTS ***
LLVM and Clang are released under the University of Illinois/NCSA
Open Source License by the LLVM Team (http://llvm.org). See the 
LLVM license for additional details. 

2)   BUILD AND INSTALLATION

LLVM and Clang are absolutely required to run h2m. The h2mbuild.sh
script can handle the installation of this software if necessary.
If LLVM and Clang are already installed, CMake will need information
about where their header and library files are installed.
The easiest way to provide this information is to provide the 
paths to the directories containing the LLVMConfig.cmake and
ClangConfig.cmake files which will provide all the needed variables
to complete a build. If these files do not exist, manual paths to
library and header files can be specified.

Usage of the h2mbuild.sh script











