#!/bin/sh

# Reports the error condition and exits
error_report ()
{
  echo "Error. $1">&2
  exit 1
}

if [ "$1" == "-help" ]
then
  echo "h2mbuild options:"
  echo "-download downloads and installs llvm and clang"
  echo "download will be acquired from github and written"
  echo "to created directory 'clang-llvm' unless specified"
  echo "with -download_path"
  echo "-download_path may be a relative or absolute path"
  echo "to the directory where the llvm/clang download"
  echo "and installation will take place"
fi

# If there is a requested download, commence!
if [ "$1" == "-download" ]
then
  install_dir=./clang-llvm
  start_dir="$PWD"

  # Obtain source code from git
  #TODO: check for existing directory
  mkdir "$install_dir" || error_report "Can't create $install_dir"
  cd "$install_dir" || error_report "Can't change to $install_dir"
  git clone http://llvm.org/git/llvm.git || error_report "Can't clone http://llvm.org/git/llvm.git"
  cd llvm/tools || error_report "Can't change to llvm/tools"
  git clone http://llvm.org/git/clang.git || error_report "Can't clone http://llvm.org/git/clang.git"
  cd "$start_dir" || error_report "Can't change to $start_dir"

  # Install clang and llvm
  mkdir "$install_dir"/clang-llvm/build || error_report "Can't create $install_dir/clang-llvm/build"
  cd "$intall_dir"/clang-llvm/build || error_report "Can't change to build directory"
  cmake -G "Unix Makefiles" ../llvm || error_report "CMakeError"
  make || error_report "Make error"

  # Make h2m
  cd "$start_dir" || error_report "Can't change to $start_dir"
  cmake . -DClangDIR="$install_dir"/build/lib/cmake/clang -DLLVMDIR="$intall_dir"/build/lib/cmake/llvm || exit
  make

fi 
