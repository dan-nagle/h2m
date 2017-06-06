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
Always run ./h2mbuild.sh. Do not run the script from a different working 
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
at NCAR by Sisi Liu and revised by Michelle Anderson and 
*** VARIOUS ACKNOWLEDGMENTS ***
LLVM and Clang are released under the University of Illinois/NCSA
Open Source License by the LLVM Team (http://llvm.org). See the 
LLVM license for additional details. 

2)   BUILD AND INSTALLATION

General Information:
LLVM and Clang are absolutely required to run h2m. The h2mbuild.sh
script can handle the installation of this software if necessary.
If LLVM and Clang are already installed, CMake will need information
about where their header and library files are installed.
The easiest way to provide this information is to provide the 
paths to the directories containing the LLVMConfig.cmake and
ClangConfig.cmake files which will provide all the needed variables
to complete a build. If these files do not exist, manual paths to
library and header files can be specified.
Because the h2mbuild.sh script must change directories, always run
it as ./h2mbuild.sh. It will not work correctly if run from a 
different working directory (although it will not cause any damage,
it will not complete the installation process).

Usage of the h2mbuild.sh script:
Usage: ./h2mbuild.sh [options]
Options:

-i	Invokes an interactive build. All variables will be specified
interactively. Other options will be ignored.

-download	Requests that LLVM and Clang be downloaded and built.

-install_dir [path]	Option to specify the directory where 
Clang and LLVM will be downloaded and built if -download is specified.	
This option will be ignored if -download was not specified.

-LLVM_DIR [path]	Option to specify the absolute path to the 
location of the LLVMConfig.cmake file if LLVM is already installed 
(ie if LLVMConfig.cmake is installed in /Users/lib/cmake/LLVM, specify
-LLVM_DIR /Users/lib/cmake/LLVM). If this option is found, manual path
specifications will be ignored (ie LLVM_LIB_PATH will be ignored).
This option will be ignored if -download is specified.

-CLANG_DIR [path]	Option to specify the absolute path to the
location of the ClangConfig.cmake file if Clang is already installed
(ie if ClangConfig.cmake is installed in /Users/lib/cmake/Clang, specify
-CLANG_DIR /Users/lib/cmake/Clang). If this option is found, manual
path specifications will be ignored (ie CLANG_LIB_PATH will be ignored.)
This option will be ignored if -download is specified.

-LLVM_LIB_PATH [path]		Option to specify the absolute path to the 
location of LLVM library files (either .a or .so files). The directory
specified should contain libLLVMArch64AsmParser to libLLVMipo. This
only needs to be provided if LLVMConfig.cmake cannot be found. If
this option is specified, -LLVM_INCLUDE_PATH must also be provided.
This option will be ignored if -download is specified.

-LLVM_INCLUDE_PATH [path]	Option to specify the absolute path to
the location of LLVM include files (.h files). The directory specified
should contain subdirectories ADT, Analysis, AsmParser... Transforms, XRay.
This option only needs to be provided if LLVMConfig.cmake cannot be
found. If this options is specified, -LLVM_LIB_PATH must also be provided.
This option will be ignored if -download is specified.

-CLANG_LIB_PATH	[path]		Option to specify the absolute path to 
the Clang libraries. The directory specified should contain libclang... 
libclangToolingRefactor. These may be .a or .so files. This option only 
needs to be specified if ClangConfig.cmake cannot be found. If this 
option is specified, -CLANG_BUILD_PATH and -CLANG_INCLUDE_PATH must 
also be provided. This option will be ignored if -download is specified.

-CLANG_BUILD_PATH [path]	Option to specify the absolute path to
the Clang build directory which has some necessary include files in it.
This directory needs to contain ./clang/AST/DeclNodes.inc. This is 
probably .../build/tools/clang/include on standard builds. This option
only needs to be specified if ClangConfig.cmake cannot be found. If
this option is specified, -CLANG_LIB_PATH and CLANG_INCLUDE_PATH
must also be specified. This option will be ignored if -download
is specified.

-CLANG_INCLUDE_PATH [path]	Option to specify the absolute path to
the Clang include directory. This directory needs to contain ./clang/AST...
./clang/StaticAnalyzer, usually llvm/tools/clang/include on standard
builds. This option only needs to be specified if ClangConfig.cmake
cannot be found. If this option is specified, -Clang_LIB_PATH and
-CLANG_INCLUDE_PATH must also be specified. This option will be
ignored if -download is specified.

-tools		Option to request the download and build of additional 
Clang tools along with Clang and LLVM. Note this is only supported with
git downloads and will be ignored if curl is specified. This option 
will be ignored if -download is not specified.

-install	Requests attempted installation of the software LLVM
and Clang. This will be standard, default installation. If specialized
installation locations are necessary, run make install seperately after 
the build is complete. This option will be ignored if -download is 
not specified.

-curl		Requests a download using curl rather than git. Note
that the download of extra Clang tools is not supported with Curl.
This option will be ignored if -download is not specified.

-LLVM_URL [url]		Option to specify an alternative path to the
download location for LLVM. For git downloads the default URL is
https://llvm.org/git/llvm.git. For Curl downloads the default URL
is https://github.com/llvm-mirror/llvm/archive/master.zip.
This option will be ignored if -download is not specified.

-CLANG_URL [url]	Option to specify an alternative path to
the download location for Clang. For git downloads the default URL
is https://llvm.org/git/clang.git. For Curl downloads, the default
URL is https://github.com/llvm-mirror/clang/archive/master.zip.
This optoin will be ignored if -download is not specified.

-TOOLS_URL [url]	Option to specify an alternative path to
the download location for the extra clang tools. Note that tools
download is only supported using git. The default URL is
https://llvm.org/git/clang-tools-extra.git. This option will
be ignored if -download is not specified.

3)   USAGE OF H2M

The h2m Autofortran tool is invoked at the shell as ./h2m [path to header].
The output translation will appear on standard output and can be sent to
a written to a file with standard shell redirection: 
./h2m [path to header] > [file to write]

4)   KNOWN ISSUES

Warnings are not yet implemented for C features which have no Fortran 
equivalent. Note that names in Fortran may not begin with an underscore,
"_" but this is legal in C.

Clang warnings may be generated during translation. This does not mean
that there is necessarilly something wrong with the code in question.



