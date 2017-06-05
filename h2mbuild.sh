#!/bin/sh

# Reports the error condition and exits
error_report ()
{
  echo "Error. $1">&2
  exit 1
}

# Function which prints help information
print_help ()
{
  echo "h2mbuild options:"
  echo "-download downloads and installs llvm and clang"
  echo "-install_dir gives the path to a download location"
  echo "-LLVM_DIR gives the path to an existing LLVM cmake config file"
  echo "-CLANG_DIR gives the path to an existing Clang cmake config file"
  echo "-LLVM_LIB_PATH gives the path to existing LLVM library files"
  echo "-LLVM_INCLUDE_PATH gives the path to existing LLVM header files"
  echo "-CLANG_LIB_PATH gives the path to existing Clang library files"
  echo "-CLANG_BUILD_PATH gives the path to an existing Clang build directory"
  echo "-CLANG_INCLUDE_PATH gives the path to existing Clang header files"
  echo "-tools requests the download of additional clang tools."
  echo "-install requests attempted installation of the software"
  echo "See README.txt for additional details."
  exit 0
}

# Process command line args via shift
download=no  # Default is no installation of clang/llvm
install_dir="./clang-llvm"  # Default installation directory is ./clang-llvm
LLVM_DIR=
CLANG_DIR=
LLVM_LIB_PATH=
LLVM_INCLUDE_PATH=
CLANG_LIB_PATH=
CLANG_BUILD_PATH=
CLANG_INCLUDE_PATH=
tools=
install=
while [ $# -gt 0 ]
do
  case "$1" in
    -help) print_help();;  # Print help information and exit
    -download) download=yes;;  # A download is requested
    -install_dir) install_dir="$2"; shift;;  # Location to write clang/llvm
    -LLVM_DIR) LLVM_DIR="$2"; shift;;  # Location of existing llvm information
    -CLANG_DIR) CLANG_DIR="$2"; shift;;  # Location of existing clang information
    -LLVM_LIB_PATH) LLVM_LIB_PATH="$2"; shift;;  # Path to LLVM library in existence
    -LLVM_INCLUDE_PATH) LLVM_INCLUDE_PATH="$2"; shift;;  # Path to LLVM include in existence
    -CLANG_LIB_PATH) CLANG_LIB_PATH="$2"; shift;;  # Path to existing Clang library
    -CLANG_BUILD_PATH) CLANG_BUILD_PATH="$2"; shift;;  # Path to existing Clang build files
    -CLANG_INCLUDE_PATH) CLANG_INCLUDE_PATH="$2"; shift;;  # Path to existing Clang include files
    -tools) tools=yes;;  # Download optional clang tools
    -install) install=yes;;
  esac
  shift
done


# If there is a requested download, commence!
if [ "$download"  == "yes" ]
then
  start_dir="$PWD"

  # Obtain source code from git
  if [ ! -d "$install_dir" ]  # Checks to make sure the directory doesn't exit before creating
  then
    mkdir "$install_dir" || error_report "Can't create $install_dir"
  fi
  cd "$install_dir" || error_report "Can't change to $install_dir"
  git clone http://llvm.org/git/llvm.git || error_report "Can't clone http://llvm.org/git/llvm.git"
  cd llvm/tools || error_report "Can't change to llvm/tools"
  git clone http://llvm.org/git/clang.git || error_report "Can't clone http://llvm.org/git/clang.git"
  if [ "$tools" == "yes" ]  # Download additional clang tools as requested
  then
    cd clang/tools || error_report "Unable to change to clang/tools directory"
    git clone http://llvm.org/git/clang-tools-extra.git extra || error_report "Can't clone clang tools"
  fi
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # We clone the software and return to our initial working directory

  # Install clang and llvm
  mkdir "$install_dir"/clang-llvm/build || error_report "Can't create $install_dir/clang-llvm/build"
  cd "$intall_dir"/clang-llvm/build || error_report "Can't change to build directory"
  cmake -G "Unix Makefiles" ../llvm || error_report "CMakeError"
  make || error_report "Make error"
  if [ "$install" == "yes" ]  # Attempted installation requested
  then
    make install || error_report "Unable to install clang and llvm"
  fi

  # Make h2m, knowing the location of the installations' cmake configuration files
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # Attempt to make the software. Note that cmake probably provides enough error reporting
  # The most likely problem would be with a change in installation location with a latter clang/llvm release
  cmake . -DClangDIR="$install_dir"/build/lib/cmake/clang -DLLVMDIR="$intall_dir"/build/lib/cmake/llvm || exit 1
  make  || exit 1
  if [ "$install" == "yes" ]  # Attempted installation of software requested
    make install || error_report "Unable to install h2m software"
  exit 0
fi 
# Installation and configuration are finished if an installation was requested... otherwise...

# The cmake_command will be built up piece by piece according to requested options
cmake_command=
if [ "$LLVM_DIR" ]  # Include LLVM location specs in the command
then
  cmake_command="$cmake_command -DLLVM_DIR=$LLVM_DIR"
fi
if [ "$CLANG_DIR" ]  # Include Clang location specs in the command
then
  cmake_command="$cmake_command -DCLANG_DIR=$CLANG_DIR"
fi
if [ "$LLVM_LIB_PATH" && "$LLVM_INCLUDE_PATH" ]  # Check for all manual LLVM specs
then
  cmake_command="$cmake_command -DLLVM_INCLUDE_PATH=$LLVM_INCLUDE_PATH -DLLVM_LIB_PATH=$LLVM_LIB_PATH"
else if [ "$LLVM_LIB_PATH" || "$LLVM_INCLUDE_PATH" ]  # One is set but not both; We need both
  error_report "Invalid options. Both LLVM_INCLUDE_PATH and LLVM_LIB_PATH are needed"
fi
if [ "$CLANG_LIB_PATH" && "$CLANG_INCLUDE_PATH" && "$CLANG_BUILD_PATH" ] # Check for all manual clang specs
  cmake_command="$cmake_command -DCLANG_INCLUDE_PATH=$CLANG_INCLUDE_PATH"
  cmake_command="$cmake_command -DCLANG_LIB_PATH=$CLANG_LIB_PATH"
  cmake_command="$cmake_command -DCLANG_BUILD_PATH=$CLANG_BUILD_PATH"
else if [ "$CLANG_LIB_PATH" || "$CLANG_INCLUDE_PATH" || "$CLANG_BUILD_PATH" ]  # We need all three or none set
  error_report "Invalid options. CLANG_LIB_PATH, CLANG_INCLUDE_PATH, and CLANG_BUILD_PATH are all needed"
fi

echo "$cmake_command"  # Debugging option

cmake . "$cmake_command" || exit 1

exit 0


