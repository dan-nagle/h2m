                          h2m Autofortran Tool Readme
______________________________________________________________________

Contents:
  0) Quick Start
  1) Introduction and Credits
  2) Build and Installation
     General information
     Usage of the h2mbuild.sh script
     Troubleshooting
  3) Usage of h2m
     Behavior
     Options
  4) Known Issues

0)   Quick Start
Always run ./h2mbuild.sh. Do not run the script from a different working 
directory. 

To build h2m if you do not have Clang or LLVM installed, run:
./h2mbuild.sh -download -install_dir [path to desired Clang/LLVM download]
or run ./h2mbuild.sh -i for interactive mode.

To build h2m if you have Clang and LLVM installed, run:
./h2mbuild.sh -CLANG_DIR [path to directory containing ClangConfig.cmake]
-LLVM_DIR [path to directory containing LLVMConfig.cmake]
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
are no Fortran equivalents will not be translated and warnings 
will be written to standard error.
The h2m Autofortran tool is built into Clang, the LLVM C compiler.
During translation, the Clang abstract syntax tree is used to 
assemble information about the header file. Clang is a very strict
compiler and will often generate warnings about the C code during
this phase. Standard Clang options can be specified.

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
it will not complete the build process).

Usage of the h2mbuild.sh script:
Usage: ./h2mbuild.sh [options]
Options:

-i	Invokes an interactive build. All variables will be specified
interactively. Other options will be ignored. Do not use tilde expansion,
ie "~/Documents", in interactively specified paths.

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

-install	Requests attempted installation of the software LLVM
and Clang. This will be standard, default installation. If specialized
installation locations are necessary, run make install seperately after 
the build is complete. This option will be ignored if -download is 
not specified.

-LLVM_URL [url]		Option to specify an alternative path to the
download location for LLVM. The default URL is:
https://github.com/llvm-mirror/llvm/archive/master.zip.
This option will be ignored if -download is not specified.

-CLANG_URL [url]	Option to specify an alternative path to
the download location for Clang. The default URL is:
http://releases.llvm.org/4.0.0/cfe-4.0.0.src.tar.xz
This option will be ignored if -download is not specified.

-install_h2m		Option to request an installation of the
h2m binary to a specified directory. The default is /usr/local/bin.

-INSTALL_H2M_DIR [path]		Option to specify a different directory
to install the h2m binary. A path relative to the current directory
can be supplied, but an absoulte path is recommended.

Troubleshooting

1. It can cause problems if an older version of Clang or LLVM is
used with a newer version of the other. Check to make sure there
is not a mismatch between versions if linker errors are seen.

2. Multiple copies of some files exist within the LLVM/Clang
build tree. Make sure, if manual paths are specified, that the directory
given has the proper structure (ie, the headers or library files are in
the positions mentioned in this document).

3. LLVM/Clang packages will be searched for in standard places before
CMake even looks at the manual configuration variables specified. If CMake
gives a message about unused variables, this often indicates that a cached
variable is being used or the package is in a default location and has
been found. This often results in incompatible versions and linker errors.

4. Newer versions of Clang/LLVm may be incompatible with the h2m
implementation. Release 4.0 is known to work, and this is the 
default download URL.

5. If Clang suffers a fatal error during an attempted compilation (this
may include failure to locate header files) the program will stop and
the output file will not be produced.

3)   USAGE OF H2M

The h2m Autofortran tool is invoked at the shell as ./h2m [path to header].
By default h2m sends the translated text to standard output. This can be
redirected with an option or the shell redirection operators.

./h2m -out=[path to output file] [path to header] -- [Clang options]

BEHAVIOR

The h2m autofortran tool will attempt to translate struct, function, macros, enum,
and variable declarations. It is not designed to translate program code. For
example, if a function definition is provided, it will be commented out in
the translated fortran.
Generally, any feature that cannot be translated due to an invalid name or limitations
with the -q option. Warnings about invalid names or commented out lines will be
printed to standard error unless supressed.
Each file is translated into exactly one fortran module. If necessary, USE statements
will link them based on dependencies in the header files.

Errors: In the case of some errors, the program will terminate and the output file
will be deleted. Most, however, are expected to be minor and processing will
continue.

Macros: Because fortran has no equivalent to the C macro, macros are traslated
into functions. However, because types often cannot be determined for macros,
a translation attempt may fail. In this case, the line will be commented out
and a warning will be printed to standard error. These warnings can be silenced
with the -q or -s option.

Duplicate module names: If multiple header files are found during recursive inclusion,
a suffix of _[number of repetition] will be appended to create a unique module name.

File Names Begining With "_": Because _ is not a legal character at the begining of 
a fortran name, the prefix h2m will be prepended to create a unique module name.

Unrecognized Types: When h2m does not recognize a type in a header, it will print a
warning about the problem and surround the questionably translated text with 
unrecognized_type(...) in the output.

OPTIONS:

-out=<string>		The output file for the tool can be specified here as either a
relative or absolute path.

-q			Supress warnings related to lines which have been commented out,
usually statements, function definitions, comments, or unsupported macros. Errors involving 
unrecongized types, invalid names, critical errors such as failure to open the output file,
and Clang errors will still be reported.

-s			Supress warnings related to lines which have been commented out
as well as warnings related to unrecognized types and invalid names. Critical errors,
such as failure to open the output file, and Clang errors will still be reported.

-r			Recursively search through include files found in the main file. 
Run the translation on them in reversed order and link the produced modules with USE
statements. Any given file will be included exactly once unless it cannot be found in
which case it will not be translated and future modules will have the USE statement 
corresponding to the failed translation commented out. In the case that an error is reported
by Clang, the output may be missing, corrupted, or completely usable depending on the
nature of the error. However, all following modules will have the USE statement corresponding
to that module commented out. 

-optimist		Ignore errors during the information gathering phase where the tool
determines the identities and orders of header files to recursively process. The output file
will also not be deleted regardless of what errors may occur. This option may potentially
create unusable code.

-no-system-headers	During recursive processing, ignore all system header files.

Clang Options: Following specification of the input file, options after a "--" are passed
as arguments to the Clang compiler instance used by the tool. The Clang/LLVM manual pages
and websites should be used as a reference for these options.


4)   KNOWN ISSUES

Clang warnings may be generated during translation. This does not mean
that there is necessarilly something wrong with the code in question.



