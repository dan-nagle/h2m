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
  echo "-i interactive. All variables specified interactively."
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
interactive=


while [ $# -gt 0 ]
do
  case "$1" in
    -help) print_help;;  # Print help information and exit
    -i) interactive="yes";;  # Obtain options interactively.
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
    *) error_report "Invalid option. Invoke ./h2mbuild.sh -help for option information"
  esac
  shift
done

# Get initial download information interactively. The rest can be obtained later
# when we begin the Cmake configuration
if [ "$interactive" == "yes" ] 
then
  echo "Interactive configuration in progress." 
  echo "Download and install llvm and clang?"
  while [ "$download_install_temp" != "y" ] && [ "$download_install_temp" != "n" ]
  do
    echo "y or n required"
    read download_install_temp
  done
  if [ "$download_install_temp" == "y" ]  # Obtain extra installation options 
  then
    download=yes  # An actual download has been requested
    echo "Download directory for clang and llvm?" 
    read install_dir
    echo "Install additional clang tools? (y/n)"
    while [ "$extra" != "y" ] && [ "$extra" != "n" ]
    do
      echo "y or n required"
      read extra 
    done
    echo "Attempt installation of software into binary directories? (y/n)"
    while [ "$install_attempt" != "y" ] && [ "$install_attempt" != "n" ]
    do
      echo "y or n required"
      read install_attempt
    done

    # Sort the obtained input into usable options for installation
    install_dir="$install_dir"  # The given download directory
    if [ "$extra" == "y" ]
    then
      extra="yes"
    fi 
    if [ "$install_attempt" == "y" ]  # Whether to run make install or not
    then
       install="yes"
    fi
  fi  # End obtaining options for the download location etc
fi  # End interactive processing

# If there is a requested download, commence!
if [ "$download"  == "yes" ]
then
  echo "Beginning download to $install_dir of clang and llvm"
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

  # Build clang and llvm
  echo "Building clang and llvm"
  mkdir "$install_dir"/clang-llvm/build || error_report "Can't create $install_dir/clang-llvm/build"
  cd "$intall_dir"/clang-llvm/build || error_report "Can't change to build directory"
  cmake -G "Unix Makefiles" ../llvm || error_report "CMakeError"
  make || error_report "Make error"
  if [ "$install" == "yes" ]  # Attempted installation requested
  then
    echo "Attempting to install llvm and clang"
    make install || error_report "Unable to install clang and llvm"
  fi

  # Make h2m, knowing the location of the installations' cmake configuration files
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # Attempt to make the software. Note that cmake probably provides enough error reporting
  # The most likely problem would be with a change in installation location with a latter clang/llvm release
  echo "Attempting to build h2m using default cmake configuration file locations"
  cmake . -DClangDIR="$install_dir"/build/lib/cmake/clang -DLLVMDIR="$intall_dir"/build/lib/cmake/llvm || exit 1
  make  || exit 1
  if [ "$install" == "yes" ]  # Attempted installation of software requested
  then
    echo "Attempting installation of h2m"
    make install || error_report "Unable to install h2m software"
  fi
  exit 0
fi 
# Installation and configuration are finished if an installation was requested... otherwise...

# Request information about installation paths
if [ "$interactive" == "yes" ]
then
  echo "Do you need to specify specialized clang/llvm installation information for cmake?"
  echo "If cmake configuration files for llvm and clang are not in a standard location,"
  echo "you will need to provide some sort of information."
  while [ "$need_specify" != "y" ] && [ "$need_specify" != "n" ]
  do
    echo "y/n required"
    read need_specify
  done
  if [ "$need_specify" == "y" ]
  then
    echo "Specify paths to cmake configuration files for LLVM?"
    echo "otherwise library and include paths are needed"
    while [ "$use_llvm_config" != "y" ] && [ "$use_llvm_config" != "n" ]
    do
      echo "y/n required"
      read use_llvm_config
    done
    echo "Specify paths to cmake configuration files for Clang?"
    echo "otherwise library and include paths are needed"
    while [ "$use_clang_config" != "y" ] && [ "$use_clang_config" != "n" ]
    do
      echo "y/n required"
      read use_clang_config
    done
  fi  # End aquisition of information about what paths to obtain
  if [ "$use_llvm_config" == "y" ]  # Obtain directory path to LLVMConfig.cmake file
  then
    echo "Specify path to LLVM configuration file"
    read LLVM_DIR
  else
    echo "Specify path to LLVM include files"
    read LLVM_INCLUDE_PATH
    echo "Specify path to LLVM library files"
    read LLVM_LIB_PATH
  fi
  if [ "$use_clang_config" == "y" ]  # Obtain directory path to ClangConfig.cmake
  then 
    echo "Specify path to Clang cmake configuration file"
    read CLANG_DIR
  else 
    echo "Specify path to Clang library files"
    read CLANG_LIB_PATH
    echo "Specify path to Clang include files"
    read CLANG_INCLUDE_PATH
    echo "Specify path to Clang build files"
    read CLANG_BUILD_PATH
  fi
fi  # End interactive section

# The cmake_command will be built up piece by piece according to requested options
cmake_command=

if [ "$LLVM_DIR" ]  # Include LLVM location specs in the command
then
  echo "Configuring cmake LLVM information:"
  echo "LLVM_DIR=$LLVM_DIR"
  cmake_command="$cmake_command -DLLVM_DIR=$LLVM_DIR"
fi
if [ "$CLANG_DIR" ]  # Include Clang location specs in the command
then
  echo "Configuring cmake Clang information:"
  echo "CLANG_DIR=$CLANG_DIR"
  cmake_command="$cmake_command -DCLANG_DIR=$CLANG_DIR"
fi

if [ "$LLVM_LIB_PATH" ] && [ "$LLVM_INCLUDE_PATH" ]  # Check for all manual LLVM specs
then
  echo "Manually configuring llvm installation information:"
  echo "LLVM_LIB_PATH=$LLVM_LIB_PATH, LLVM_INCLUDE_PATH=$LLVM_INCLUDE_PATH"
  cmake_command="$cmake_command -DLLVM_INCLUDE_PATH=$LLVM_INCLUDE_PATH -DLLVM_LIB_PATH=$LLVM_LIB_PATH"
elif [ "$LLVM_LIB_PATH" ] || [ "$LLVM_INCLUDE_PATH" ]  # One is set but not both; We need both
then
  error_report "Invalid options. Both LLVM_INCLUDE_PATH and LLVM_LIB_PATH are needed"
fi

# Include manual clang paths and check to make sure options have been passed properly
if [ "$CLANG_LIB_PATH" ] && [ "$CLANG_INCLUDE_PATH" ] && [ "$CLANG_BUILD_PATH" ]
then
  echo "Manually configuring clang installation information:"
  echo "CLANG_LIB_PATH=$CLANG_LIB_PATH, CLANG_INCLUDE_PATH=$CLANG_INCLUDE_PATH,"
  echo "CLANG_BUILD_PATH=$CLANG_BUILD_PATH"
  cmake_command="$cmake_command -DCLANG_INCLUDE_PATH=$CLANG_INCLUDE_PATH"
  cmake_command="$cmake_command -DCLANG_LIB_PATH=$CLANG_LIB_PATH"
  cmake_command="$cmake_command -DCLANG_BUILD_PATH=$CLANG_BUILD_PATH"
# We need all three or none set
elif [ "$CLANG_LIB_PATH" ] || [ "$CLANG_INCLUDE_PATH" ] || [ "$CLANG_BUILD_PATH" ]
then
  error_report "Invalid options. CLANG_LIB_PATH, CLANG_INCLUDE_PATH, and CLANG_BUILD_PATH are all needed"
fi

# Attempt to execute the cmake commands to create h2m
echo "cmake command is $cmake_command"  # Debugging line

cmake . "$cmake_command" || exit 1 # Disabled for debugging

if [ "$install" == "yes" ]  # Attempt to install h2m
then
  echo "Attempting to install h2m"
  make install || error_report "Unable to install h2m"
fi

exit 0


