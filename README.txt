                          h2m Autofortran Tool Readme
______________________________________________________________________

Contents:
  0) Quick Start
  1) Introduction and Credits
  2) Build and Installation
     General Information
     Usage of the h2mbuild.sh Script
     Building h2m on Supercomputers
     Building h2m Without the Script
     Troubleshooting
  3) Usage of h2m
     Behavior
     Options
     Example
     Grooming Produced Files
  4) Known Issues

0)   Quick Start
Always run ./h2mbuild.sh. Do not run the script from a different working 
directory. 

To build h2m if you do not have Clang or LLVM installed, run:
./h2mbuild.sh -download -download_dir [path to desired Clang/LLVM download]
or run ./h2mbuild.sh -i for interactive mode.

It is recommended to download and install CMake seperately from h2m, but 
the script may be able to do this for you.
To build h2m if you are also missing CMake as well as Clang and LLVM, run:
./h2mbuild.sh -download -download_dir [path to desired Clang/LLVM download]
-download_cmake -download_cmake_dir [path to desired CMake download]

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
During translation, the Clang abstract syntax tree (AST) is used to 
assemble information about the header file. Clang is a very strict
compiler and will often generate warnings about the C code during
this phase. Standard Clang options can be specified to control the
compilation process and silence warnings.

The h2m Autofortran tool was envisioned by Dan Nagle and completed
at NCAR by Sisi (Garnet) Liu and revised by Michelle Anderson. Special
thanks to Davide Del Vento.
LLVM and Clang are released under the University of Illinois/NCSA
Open Source License by the LLVM Team (http://llvm.org). See the 
LLVM license for additional details. 
CMake is released under the OSI-approved BSD 3-clause License, originally
developed by Kitware Inc (https://cmake.org). See the CMake license for
additional details. CMake distributions include third party software
with compatible licenses.

2)   BUILD AND INSTALLATION

General Information:
LLVM and Clang are very demanding on compilers and libraries used
to build them. A recent version of gcc or Clang is recommended for
the build process. The gcc version must be at least 4.8. 
The utilities required to build and run h2m are: CMake (preferably
a very recent version), LLVM, Clang, curl or wget (only if downloads
of CMake, LLVM, and Clang are needed), bash, and standard Unix utilities.
Utilities required to build LLVM are enumerated on the website
llvm.org/docs/GettingStarted.html.
Though it was principally designed for bash shells, h2mbuild.sh may
run under other shells including ksh and zsh. Other posix compliant
shells may also work, but they have not been tested.
LLVM and Clang are absolutely required to run h2m. The h2mbuild.sh
script can handle the installation of this software if necessary. In
fact, it is most convenient if Clang and LLVM are downloaded and built
during the installation process to avoid potential complications and
avoid the need to determine the locations of their config.cmake files.
However, this may not always be possible because building these large
projects is not always smooth and the script can only provide basic
build options.
Note that there is no "bootstrapping" involved in the build carried
out by the script. Its only goal is to compile header and library
files used by h2m as quickly as possible, not to create a reliable
and thoroughly tested compiler.
If CMake is not installed, (ie the command 'which cmake' fails) CMake
will need to be installed. The build script can perform a "bare-bones"
installation if requested.
If LLVM and Clang are already installed, CMake will need information
about where their header and library files are located.
The easiest way to provide this information is to provide the 
paths to the directories containing the LLVMConfig.cmake and
ClangConfig.cmake files. These will provide all the needed variables
to complete a build. If these files do not exist, manual paths to
library and header files can be specified.
Because the h2mbuild.sh script may change directories, always run
it as ./h2mbuild.sh. It will not work correctly if run from a 
different working directory (although it will not cause any damage,
it will not complete the build process). The script should fail with
an error if not run from the proper directory.

Usage of the h2mbuild.sh script:
Usage: ./h2mbuild.sh [options]
Options:

-i	Invokes an interactive build. All variables will be specified
interactively. Other options will be ignored. Do not use tilde expansion,
ie "~/Documents", in interactively specified paths.

-f	When performing tests to assess whether legal compilers exists,
do not stop or print a warning if the test fails.

-download_cmake		Requests that CMake be downloaded and installed.
This will likely require administrator privileges. Alternately, CMake 
can be downloaded from https://cmake.org/download/ as a binary release
(not automated by this script.) The default directory if none is specified
is ./cmake_dir for the download.

-download_cmake_dir	Option to specify the absolute or relative path
to the desired download location for CMake. CMake files will be extracted
into the CMake subdirectory of this directory.  This option will be ignored
if -download_cmake is not specified.

-CMAKE_INSTALL_PREFIX	If CMake is built, set the installation prefix
to this directory. This allows control over the installation location.

-download	Requests that LLVM and Clang be downloaded and built.
The default directory if none is specified is ./clang-llvm

-download_dir [path]	Option to specify the directory where 
Clang and LLVM will be downloaded and built if -download is specified.	
This option will be ignored if -download was not specified.

-LLVM_DIR [path]	Option to specify the absolute path to the 
location of the LLVMConfig.cmake file if LLVM is already built 
(ie if LLVMConfig.cmake is present in /Users/lib/cmake/LLVM, specify
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
installation locations are necessary, run make install separately after 
the build is complete. This option will be ignored if -download is 
not specified.

-LLVM_URL [url]		Option to specify an alternative to the default
download location for LLVM. The default URL is:
http://releases.llvm.org/4.0.0/llvm-4.0.0.src.tar.xz
This option will be ignored if -download is not specified.

-CLANG_URL [url]	Option to specify an alternative to the default
download location for Clang. The default URL is:
http://releases.llvm.org/4.0.0/cfe-4.0.0.src.tar.xz
This option will be ignored if -download is not specified.

-CMAKE_URL [url]	Option to specify an alternative to the default
download location for CMake. The default URL is:
https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz.
This option will be ignored if -download_cmake is not specified.

-install_h2m		Option to request an installation of the
h2m binary to a specified directory. The default destination is
/usr/local/bin if none is specified.

-INSTALL_H2M_DIR [path]		Option to specify a different directory
to install the h2m binary. A path relative to the current directory
can be supplied, but an absolute path is recommended.

Building h2m On Supercomputers:

The h2mbuild.sh script and CMakeLists.txt script should ideally run on 
supercomputers exactly as described throughout this section.
However, in practice it may be much more difficult to build h2m on a
supercomputer. This section contains some special advice for this
more complicated build process.

1. Minimize libraries that might accidentally be linked into a build
This will likely mean unloading all the modules that are not 
absolutely necessary to build h2m (probably meaning compiler packages and
CMake). If the system where the build is being attempted does not use 
modules, more complicated and individualized methods for avoiding
the inclusion of bad library references into a build may be necessary.
Every effort should be made to keep the linker from adding improper
references. If the build ends with a warning about a missing .so
library or h2m fails to execute because the dynamic linker cannot
resolve references to a library, a bad reference has probably been
introduced into the build of LLVM or Clang. The build process will
need to be restarted (the build with the bad reference is useless)
and additional efforts to keep bad libraries out of the build will
be necessary.

2. Check the available compilers and versions
LLVM and Clang are extremely demanding of the compilers that build them.
Recent versions of gcc and Clang compilers should be acceptable. Check
the LLVM website to make sure that the compiler available is not one
of those known to contain a bug resulting in build failure. No version
of the Intel C compiler is likely to correctly build LLVM and Clang
and it should never be used.

3. Check the compiler found by CMake
If there are several compilers available, CMake may choose the wrong one.
CMake should inform you of the compiler and version used as it begins
the build process. Make sure it is the compiler you want it to use. If
CMake chooses the wrong compiler, unload the unwanted compiler's module,
change the path variable, or build Clang and LLVM manually, specifying
the desired compiler. You will need to refer to LLVM build instructions
and CMake documentation to do this.

4. Build in pieces if errors occur
It may be necessary to build CMake, Clang and LLVM, and h2m separately
due to complicated problems building the large projects. Troubleshooting
advice from the LLVM and CMake websites should be helpful. The
h2mbuild.sh script can then be used to build h2m, or CMake can be
used directly, specifying the directories to search for the CMake config
files as described directly below.


Building h2m Without the Script:

It is absolutely necessary to have CMake installed to build h2m, but 
the script is not necessarily needed. To build h2m without the script,
follow these steps.

0.5). Download and Build (or update) CMake
CMake releases can be downloaded from https://cmake.org/download.
Build and installation instructions for CMake can be found at 
https://cmake.org/install. If an older version of cmake (prior to 3.2)
is installed, it will need to be updated. Reference the CMake website
for instructions on updating to the latest version.

1). Download and Build Clang/LLVM
LLVM and Clang can be downloaded from releases.llvm.org.
To guarantee that all the needed files are present, download and 
build from source. A binary release is unlikely to have
the needed header and CMake files. Clang/LLVM version 4.0.0 is the
only version thoroughly tested. Somewhat earlier versions are likely to
work as well. Later versions almost certainly will not. The Clang 
website (https://clang.llvm.org/get_started.html) provides good 
build instructions.

2). Find the CMake Configuration Files
The files clang-config.cmake and llvm-config.cmake should be located
in [path to llvm/clang build directory]/lib/cmake/llvm and 
[path to llvm/clang build directory]/lib/cmake/clang respectively.
In case of trouble, find, (find [directory] -name=llvm-config.cmake)
can help locate these files. 
If you have LLVM and Clang installed in standard locations, you may
not need to specify paths for CMake at all.

2.5). What to do Without Configuration Files
Building h2m without LLVM and Clang configuration files is difficult
and error prone but it can be done. In this case, locate the following
directories:
[a] The directory containing ./clang/Frontend/FrontendAction.h
(in the Clang source tree)
[b] The directory containing ./clang/AST/DeclNodes.inc (in the build tree)
[c] The directory containing libclang.a or libclang.so (in the build tree)
[d] The directory containing ./ADT/APFloat.h (in the LLVM source tree)
[e] The directory containing libLLVMCore.a or libLLVMCore.so (in the build tree)

3). Run CMake, Including Needed Configuration File Paths 
In the directory including h2m.h, h2m.cpp, and CMakeLists.txt, run
'cmake . -DLLVM_DIR=[path to directory containing llvm-config.cmake]
-DClang_DIR=[path to directory containing clang-config.cmake]'. Use
absolute paths (ie /Users/me/clang-llvm/build/lib/cmake/clang not
~/clang-llvm... or ../clang-llvm).
If you would like to install h2m, add an extra argument of the form
'-DINSTALL_PATH=[path to desired installation location].

3.5). How to Run CMake Without Configuration Files
In the directory including h2m.h, h2m.cpp, and CMakeLists.txt, run the
following unpleasant command. Refer to the letters specified earlier
in step 2.5 which represent the absolute paths to the directories found
in that step:
'cmake . -DLLVM_LIB_PATH=[e] -DLLVM_INCLUDE_PATH=[d] 
-D_CLANG_INCLUDE_DIRS=[a] -D_CLANG_BUILD_PATH=[b] -D_CLANG_LIB_PATH=[c]'
Add the optional installation location (as in step 3) if desired.

4) Run Make
Simply running 'make' should build h2m. If you wish to install h2m, run
'make install' after running make.

Troubleshooting

1. It can cause problems if an older version of Clang or LLVM is
used with a newer version of the other. Check to make sure there
is not a mismatch between versions if linker errors are seen.
This advice only applies when Clang and LLVM were already available
on the computer used and were not built from scratch during the 
process.

2. Multiple copies of some files exist within the LLVM/Clang
build tree. Make sure, if manual paths are specified, that the directory
given has the proper structure (ie, the headers or library files are in
the positions mentioned in this document).

3. LLVM/Clang packages will be searched for in standard places before
CMake even looks at variables such as LLVM_INCLUDE_DIRS. If CMake
gives a message about unused variables, this often indicates that a cached
variable is being used or the package is in a default location and has
been found. This can result in incompatible versions and linker errors.
Often a clean download and build of LLVM and Clang can fix these problems.

4. Newer versions of Clang/LLVm may be incompatible with the h2m
implementation. Release 4.0 is known to work, and this is the 
default download URL. It is likely that releases after 4.0 will not
work at all.

5. LLVM and Clang are extremely demanding on the compilers used to 
build them. Later versions of gcc are known to work, as are other
versions of Clang, but icc will probably not work. See the Clang and
LLVM websites for more information about troubleshooting their
build processes. 

6. Make sure that the compiler you think is being used to build LLVM
and Clang is actually the compiler found by CMake. Alteration of 
search paths, or manual builds explicitly specifying the compiler,
may be necessary. CMake should name the found compiler and 
version as it begins the build process.

7. Always build h2m and Clang/LLVM with the same version of the
same compiler. Bad library references will cause problems if this
is not done.

8. If there are syntax errors while attempting to run the h2mbuild.sh
script, make sure that either bash, ksh, or zsh is being used to
run the script. The build script was written to be run in bash
and bash should be used if it is available.

9. An old version of CMake cannot handle the build process for Clang and
LLVM or h2m. Update CMake if necessary. Version 3.2 or later is required.
Refer to the CMake website for update and build instructions. 

10. It may be more convenient and easier to download CMake from its
website as a binary release. The script does not provide an option
to do this, but it is not too difficult to do by hand.

11. If linker warnings regarding visiblitity settings are seen (this
sometimes happens when building LLVM, Clang, and h2m with GCC) h2m
may run perfectly normally. If it does not, you may need to override
the compiler flags. This can be done by running CMake, providing all
the variables to the CMake script by hand, and altering the variable
CMAKE_CXX_FLAGS by adding -DCMAKE_CXX_FLAGS="flags flags... flags..".
The flags currently set by the project are:
"-std=c++11 -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden".


3)   USAGE OF H2M

The h2m Autofortran tool is invoked at the shell as ./h2m [path to header].
By default h2m sends the translated text to standard output. This can be
redirected with an option or the shell redirection operators.

./h2m [h2m options] -out=[path to output file] [path to header] [Clang frontend options]

BEHAVIOR

The h2m autofortran tool will attempt to translate struct, function, macros, enum,
and variable declarations. It is not designed to translate complex program code. For
example, if a function definition is provided, it will be commented out in Fortran.
Untranslatable features will be commented out. Invalid names beginning with an underscore
will be prefixed with 'h2m' to create a valid name.
Warnings about invalid names or commented out lines will be printed to standard error
unless suppressed with the -q or -s option. 
By default, only the contents of a single header file are written into a single Fortran
module. Files included by this header are ignored. There are two ways to change this.
The first way is to request a recursive run with the option -recursive or -r.
During recursion, each file is translated into exactly one Fortran module. These
modules will be linked together by USE statements, probably far more USE statements
than are actually necessary. The tool will attempt to link them in the order which
corresponds to the dependencies among the C header files. By default, system headers
will be translated into modules as well. This can be disabled with the option
-no-system-headers or -n.
Another way to change this behavior is with the -together or -t option, which will
send all local header files into a single module as the preprocessor does,
meaning all includes excluding system headers, are translated and written to a single
module. There is no option to include system headers when translating in this way.

Errors: In the case of some errors, the program will terminate and the output file
will be deleted. Most, however, are expected to be minor and processing will
continue. For example, many errors raised by Clang as it processes header files are
completely irrelevant to h2m. To make sure that the output file is kept despite
any errors, use the -keep-going or -k option. This may potentially produce incomplete
or unusable modules depending on how serious the error was.

Mistakes on the user's part may result in unusual errors. Only provide one
source file to h2m at a time or the underlying Clang infrastructure will
raise errors relating to the number of compiler jobs expected.

Some options are incompatible and will either be ignored or protested with an 
error message.

Module Names: The base file name is prepended with "module_" to create a name
for the Fortran module generated from each file. If identically named header 
files are found during recursive inclusion, a suffix of _[number of repetition]
will be appended to create a unique module name.
A module name is not checked for other illegal characters, which may well be
present depending on the user's naming scheme. The user will need to rename
the module by hand if this is the case.

Macros: Because Fortran has no equivalent to the C macro, macros are translated
approximately. However, because types often cannot be determined for macros,
a translation attempt may fail. In this case, the line will be commented out
and a warning will be printed to standard error. These warnings can be silenced
with the -q or -s option. Macros will be translated into functions, subroutines,
or parameters as most appropriate. Macros containing letter modifiers for 
numbers (u,U,L,l,f,F etc.) will also be commented out because the desired 
type is unclear.

Enumerated Types: An enumerated type of arbitrary length can be translated. Because
Fortran enums do not have their own scope as C enums do, the enumerator's name will
be commented out. Interoperability is supported for the individual enumerators 
within the enumerated type.

Typedefs: There is no Fortran type interoperable with a typedef. All typedefs will
be translated into Fortran TYPE definitions containing exactly one field. This
field will be named [typedef name]_[type redefined]. For example:
typedef int my_int;
-> h2m ->
TYPE, BIND(C) :: my_int
  my_int_C_INT
END TYPE my_int

Other Conflicting Names: The h2m tool attempts to find name conflicts as it 
translates code. It will raise warnings about conflicting names and comment
out the identifier causing the name conflict.
Name conflicts will have to be fixed manually by the programmer, either
by modifying a name or by modifying a USE statement to a USE ONLY statement
to exclude conflicting symbols.
Here is an example of a common sort of name conflict:
struct my_struct {
  int x;
};
typedef struct my_struct my_struct;

Both the struct and the typedef would be translated into Fortran TYPE definitions
with the name of my_struct because Fortran has no equivalent to C struct tag names.
The second structure definition would be a useless repetition and cause an error in
Fortran, thus h2m will print a warning and not translate the typedef.

Another cause of name conflicts is capitalization. C is case sensitive and Fortran
is not. Though h2m should catch and warn about such conflicts, the user
must manually change names to address the problem.

In all cases, comments in the code should point out the problem.

Names Beginning With "_": Names will be prepended with h2m in order to create
a legal identifier. Warnings will be printed to standard error when a name is
changed. A user must change names in the C files to allow interoperability.
Optionally, the -auto-bind or -b option can perform some Fortran to C binding
automatically. Note that these name specification statements are not legal
in structured type definitions and names of structured types must be changed
by hand.
For example:
int __my_integer;
-> h2m -b ->
INTEGER(C_INT), BIND(C, name="__my_integer") :: h2m__my_integer

Unrecognized Types: When h2m does not recognize a type in a header, it will print a
warning about the problem and surround the questionably translated text with 
WARNING_UNRECOGNIZED(...) in the output. This is most commonly seen with void types,
types associated with va_list or other built-in C types for which there is no 
Fortran equivalent, and types defined in complicated typedef statements.
The tool will attempt to detect and comment out these unrecognized types.
Warnings will be printed in the text and in standard error.

Unions: There is no Fortran type interoperable with a union. A union will be translated
into a TYPE definition in Fortran, but this translated TYPE is not interoperable.

Name and Line Lengths Exceeding Maximum:
Though C has no limits on line and name lengths, Fortran does. Though h2m can detect
line length and name length problems and will comment out the offending text, it does
not know how to split lines. This must be done by hand. Warnings will be printed on
standard error and in the translated text. These problems are most commonly seen
in typedefs with very long names and function definitions with very long argument
lists.

OPTIONS:

-array-transpose
-a			Automatically reverse the dimensions of an array translated into
Fortran (ie int x[3][5][9] becomes INTEGER, DIMENSION(9, 5, 3) :: x). Because Fortran
stores arrays in column major order and C stores arrays in row major order, this will
preserve C-like access to array members, but does not always produce proper
interoperability so use with caution.

-auto-bind
-b			Where possible, if a C name begins with an underscore, after
renaming the identifier h2m_c_identifier, insert a name="c_identifier" clause into the
BIND(C) qualifier to let the processor know that h2m_c_identifier and c_identifier are
the same entity. Note that name="c_identifier" clauses are illegal in TYPE (C struct
translations) and ENUM (C enumerated type) translations and will not be inserted.
To handle illegal names in structs or enumerations, the C name will have to be changed.

-compile=<string>
-c=<string>		Attempt to immediately compile the generated Fortran code using the
compiler command specified. If this command cannot be found, or if a command interpreter 
cannot be found, this will fail.

-hide-macros
-h			All function-like macros will be commented out rather than
translated into approximate subroutine prototypes. Macros where h2m is able to 
determine an appropriate parameter type will still be translated to Fortran
approximations.

-ignore-anon		Do not comment out TYPE definitions translated form anonymous
C structs. Warnings will still be printed. This applies only to definitions of anonymous
types. C entities which use anonymous types will still be commented out. To disable
commenting out of entities using anonyous types (ie structs containing anonymous
structs) use -ignore-type.

-ignore-duplicate	Do not comment out duplicate identifiers. Warnings will still
be printed.

-ignore-line-length	Do not comment out excessively long lines. Warnings will still
be printed.

-ignore-name-length	Do not comment out excessively long names. Warnings will still
be printed.

-ignore-this
-i			Ignore the provided file. Do not translate the given file
when performing recursive processing. This option is only valid with the 
-r/-recursive option. This is useful if a C source file includes headers which
need to be translated, but the C source file should not be translated and
should only be used to determine which other files to translate.

-ignore-type		Do not comment out illegal or unrecognized types. Warnings will
still be printed. This does not apply to definitions of anonymous types which will still
be commented out.

-keep-going
-k			Ignore errors during the information gathering phase where the tool
determines the identities and orders of header files to recursively process. The output file
will also not be deleted regardless of any errors that occur at any phase of translation
save critical errors from which the program simply cannot proceed. This can produce
unusable code.

-link-all
-l			When recursively translating code, ignore all errors that occur
during translation and never comment out USE statements. Link all modules regardless
of whether or not they are known to contain properly translated code.  Because
most Clang errors are actually minor, using this option does not normally
cause serious problems.

-no-system-headers
-n			During recursive processing, ignore all system header files.
Clang's decision as to what constitutes a system header is not always what would
be expected. 

-out=<string>
-o=<string>    		The output file for the tool can be specified here as either a
relative or absolute path.

-quiet
-q			Suppress warnings related to lines which have been commented out,
usually statements, function definitions, comments, or unsupported macros. Errors involving 
unrecognized types, invalid names, critical errors such as failure to open the output file,
and Clang errors will still be reported.

-recursive
-r			Recursively search through include files found in the main file. 
Run the translation on them in reversed order and link the produced modules with USE
statements. Any given file will be included exactly once unless it cannot be found. The
module whose include could not be found will register a Clang error and USE statements
corresponding to the failed translation will be commented out. In the case that an error is
reported by Clang, the output may be missing, corrupted, or completely usable depending on the
nature of the error. However, all following modules will have the USE statement corresponding
to that module commented out. The -l or -link-all option can override this behavior.

-silent
-s			Suppress warnings related to lines which have been commented out
as well as warnings related to unrecognized types and invalid names. Critical errors,
such as failure to open the output file, and Clang errors will still be reported.

-together
-t			Send all the local (non-system) header files to a single module as
they are translated. In this case, the entire text of the file, including all portions
added by the preprocessor, is translated into a single module. System header files will
never be included in this translation. If -recursive or -r option is also specified, this
will likely lead to symbols being defined multiple times. Do not do this.

Clang Options: Following specification of the input file, options after the source are passed
as arguments to the Clang compiler instance used by the tool. The Clang/LLVM manual pages
and websites should be used as a reference for these options.


EXAMPLE

The Example directory contains several subdirectories containing small h2m examples.
The first directory, Example_1, contains a potentially useful script which will provide
a short tutorial. The following section summarizes the process the script preforms.

Step 1: Translate the desired header file from C to Fortran using h2m:
h2m -out=module_header.f90 ./header.h

Step 2: Compile the C code into an object file. Any reliable C compiler should work, but
GCC is used for the example:
gcc  -c -I. -o c_source.o c_source.c

Step 3: Add a proper USE statement to the Fortran program:
PROGRAM example
USE, INTRINSIC :: iso_c_binding  ! Include C_DOUBLE and other interoperable KINDS
USE module_header  ! Include the translated module
...

Step 4: Call the C translations according to their Fortran interfaces in the module.
...CALL TRANSLATED_C_FUNCTION()...

Step 5: Compile the Fortran program and link against the C file. Any reliable compiler should
work, but gfortran is used as an example. The module file is compiled separately here.
It should not be necessary to explicitly include the module file in the compilation
string.
gfortran -c module_header.f90
gfortran -o example example.f90 c_source.o

Step 6: Enjoy your mixed-language executable.
./example

GROOMING PRODUCED FILES

First, note that h2m will report only one kind of error per translatable entity
regardless of how many problems there actually are. When grooming translated files,
check an entity which has been commented out thoroughly to make sure there are not
additional, unreported problems. This may occur with anonyous types, which are 
sometimes reported not as anonymous types but as in violation of name-length limits.

When translating complicated files, such as library or system headers, some problems
can come up which require human intervention. 
1. Unrecognized Types
Unrecognized types are an unavoidable nuisance in many system headers. Fortran has no
equivalent to a void type. Some intrinsically defined types such as __va_list_tags
are also unrecognized. Search for the pattern "WARNING_UNRECOGNIZED" in the created
header to find these instances.
2. Length Problems
Extremely long lines may sometimes result from function or initialization translations.
The tool will comment out all declarations containing illegal long lines.  These 
must be split up by hand. The h2m tool should warn if such long lines exist. Extremely
long names will also result in declarations being commented out, and these must be
changed by the user, either by altering the symbol's name or by inserting a name=""
specification (if legal) into the translated Fortran and shortening the Fortran
name only.
3. Pointer Initialization
Pointers in initialization lists will be set to C_NULL_PTR or C_NULL_FUNPTR as 
appropriate. They will need to be reinitialized by hand. The h2m software will
preserve their initialization values as comments.
4. Macros
Though h2m will attempt to handle simple macros, it may make mistakes even on these,
and the translations of function-like macros will need to be filled in or replaced
completely. The h2m software will preserve the text of function like macros or
untranslated macros as comments.
5. Duplicate Names
When h2m locates a repeated identifier, it comments the repeat out to avoid problems and
warns on standard error and in the comments. Names may need to be changed by hand and
the declaration altered, or it may be that this repeated declaration is actually not
needed (because it is a typedef of a structure which Fortran already recognizes by
the proper name). Note that capitalization is not significant in Fortran and this often
leads to duplicate identifiers.

4)   KNOWN ISSUES

If there are several different error conditions raised by a single line of
code, only one of them will be reported. For example, if there is an 
unrecognized type and an illegal line length detected in a function
definition, only the illegal line length will be reported on standard error
and in the output code. It is expected that the user will inspect code 
which is reported as erroneous and fix all issues.

Clang warnings may be generated during translation. This does not mean
that there is necessarily something wrong with the code in question.
When Clang raises warnings or errors on a file being translated, the translation
may be incomplete. Removing the offending pieces of the C source code, or 
altering Clang's behavior with a compiler option are two possible solutions to
this problem.

Name conflicts occur occasionally because C has different scoping rules than Fortran.
Usually, h2m will detect these conflicts and attempt to comment out the offenders.
Compiler errors such as 'Error: Symbol [symbol] cannot have a type' are likely due to 
name conflicts between module and function or type names. These name conflicts typically
lead to cascades of closely following but seemingly unrelated errors. Note that no
attempt is made to catch conflicts involving names of translated modules, but these
are exceedingly rare.

Typedefs defining 'void' will cause undefined type errors. Fortran has no reasonable
translation for a 'void' type and h2m cannot address it. Likewise, certain built-in
C types (va_list and its associates as well as associates of berval) can cause
unrecognized type problems, as can exceedingly complicated typedefs.

If Clang runs into an error pragma (#error) during preprocessing, an error will be
raised. This is unavoidable.

It can cause problems if compilers from different vendors are used to compile the C
and Fortran code pieces intended for a mixed-language executable. For example, using
icc for the C code and gfortran for the Fortran code is not advisable. It might not
cause problems, but might be disastrous due to varying rules regarding name-mangling.
If you use gcc for the C code, use gfortran for the Fortran code. Likewise, if you 
use icc for the C code, use ifort for the Fortran code.
