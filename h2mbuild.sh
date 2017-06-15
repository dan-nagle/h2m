#!/bin/bash
# Installation script for the h2m Autofortran tool.
# Original Author: Michelle Anderson.

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
  echo "-install requests attempted installation of the software llvm and clang"
  echo "-LLVM_URL gives an alternate URL from which to download LLVM"
  echo "-CLANG_URL gives an alternate URL from which to download Clang"
  echo "-TOOLS_URL gives an alternate URL from which to download Clang tools"
  echo "See README.txt for additional details."
  exit "$1"
}

# Function which determines whether or not a path is relative vs absolute 
# and returns 1 if it is absolute, 0 if relative. First argument is the
# path to be evaluated.
is_absolute()
{
  case "$1" in 
     "/"*) return 0;;  # The path started by a / is absolute
     *) return 1;;   # Otherwise it is not absolute
   esac
}

# Default values of variables which may appear on command line
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
LLVM_URL="http://releases.llvm.org/4.0.0/llvm-4.0.0.src.tar.xz"
CLANG_URL="http://releases.llvm.org/4.0.0/cfe-4.0.0.src.tar.xz"
TOOLS_URL="http://releases.llvm.org/4.0.0/clang-tools-extra-4.0.0.src.tar.xz"

# Testing for features used within this script. Failures are fatal.
which cmake || error_report "Error: CMake is needed to build LLVM/Clang and h2m."


# Process command line args via shift in a while loop
while [ $# -gt 0 ]
do
  case "$1" in
    -help) print_help 0;;  # Print help information and exit
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
    -LLVM_URL) LLVM_URL="$2"; shift;;  # The overriding address of the LLVM source
    -CLANG_URL) CLANG_URL="$2"; shift;;  # The overriding address of the Clang source
    -TOOLS_URL) TOOLS_URL="$2"; shift;;  # The overriding address of the Tools source
    *) echo "Invalid option, $1."; print_help 1;;  # Print help. Exit with error.
  esac
  shift
done

echo "$PWD/h2m.cpp"
if [ ! -f "$PWD/h2m.cpp" ]
then
  echo "Please run this script from the directory where h2m.cpp,"
  echo "h2m.h, CMakeLists.txt, and h2mbuild.sh are located."
  echo "Run ./h2mbuild.sh -help for help."
  exit 1
fi


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

    # Obtain the download directory
    echo "Download directory for clang and llvm?"
    read install_dir

    echo "Install additional clang tools? (y/n)"
    while [ "$extra" != "y" ] && [ "$extra" != "n" ]
    do
      echo "y or n required"
      read extra
    done

    # Attempts at installation for LLVM and Clang are made upon request.
    echo "Attempt installation of software (run "make install")? (y/n)"
    while [ "$install_attempt" != "y" ] && [ "$install_attempt" != "n" ]
    do
      echo "y or n required"
      read install_attempt
    done

    # Get URL information. Do we download from default sites or not?
    echo "Do you need to specify non-default download URLs?"
    while [ "$specify_urls" != "y" ] && [ "$specify_urls" != "n" ]
    do
      echo "y or n required"
      read specify_urls
    done
    if [ "$specify_urls" == "y" ]  # Obtain the special URLs as needed
    then
      echo "Provide the URL from which to download LLVM"
      read LLVM_URL
      echo "Provide the URL from which to download Clang"
      read CLANG_URL
      echo "Provide the URL from which to download Clang tools"
      read TOOLS_URL
    fi


    # Sort the obtained input into usable options for installation
    # including the download tool, download directory, and installation
    # preference.
    install_dir="$install_dir"  # The given download directory
    if [ "$extra" == "y" ]
    then
      extra="yes"
    fi
    if [ "$install_attempt" == "y" ]  # Whether to run make install or not
    then
       install="yes"
    fi
  fi  # End obtaining options for the download location and content
fi  # End interactive processing

# We start in this directory. We keep track of it for future reference.
start_dir="$PWD"

# If there is a requested download, commence!
if [ "$download"  == "yes" ]
then
  echo "Beginning download to $install_dir of clang and llvm"
  # Autodetect download tool
  which which || error_report "Command 'which' not found. Autodetection of curl/wget failed."
  if [ `which curl` ] 
  then
    echo "Curl found." 
    curl="yes"
  elif [ `which wget` ] 
  then
    echo "Wget found."
  else
    error_report "Unable to locate wget or curl to commplete download."
  fi

  # Obtain source code from the internet with Wget or Curl
  if [ ! -d "$install_dir" ]  # Checks to make sure the directory doesn't exit before creating it.
  then
    mkdir "$install_dir" || mkdir -p "$install_dir" || error_report "Can't create $install_dir"
  fi
  cd "$install_dir" || error_report "Can't change to $install_dir"
  echo "Downloading LLVM from $LLVM_URL to $install_dir/llvm.tar.xz"
  # Download from the given URL, following redirections with wget or curl as requested
  if [ "$curl" == "yes" ]
  then 
    curl -L "$LLVM_URL" > llvm.tar.xz || error_report "Unable to curl at LLVM at $LLVM_URL"
  else 
    wget -O llvm.tar.xz "$LLVM_URL" || error_report "Unable to wget at LLVM at $LLVM_URL"
  fi
  # This will filter out the name of the main folder inside the tar directory
  temp_llvm_name=`tar -tf llvm.tar.xz | head -1 | cut -f1 -d "/"` || error_report "Can't find llvm.tar.xz subdir name"
  tar -xf llvm.tar.xz || error_report "Unable to untar llvm.tar.xz"
  mv "$temp_llvm_name" llvm || error_report "Can't rename $temp_llvm_name to llvm"
  cd llvm/tools || error_report "Can't change to llvm/tools"
  # Download Clang using Curl or Wget
  echo "Downloading Clang from $CLANG_URL to $install_dir/llvm/tools/clang.tar"
  if [ "$curl" == "yes" ] 
  then
    curl -L "$CLANG_URL" > clang.tar.xz || error_report "Unable to curl at clang at $CLANG_URL"
  else
    wget -O clang.tar.xz "$CLANG_URL" || error_report "Unable to wget at clang at $CLANG_URL" 
  fi
  # This filters out the name of the main folder inside the tar directory
  temp_clang_name=`tar -tf clang.tar.xz | head -1 | cut -f1 -d "/"` || error_report "Can't find clang.tar.xz subdir name."
  tar -xf clang.tar.xz || error_report "Unable to untar clang.tar.xz"
  mv "$temp_clang_name" clang  || error_report "Unable to rename $temp_clang_name to clang"

  # If requested, the download for the clang/tools/extra directory is carried out
  if [ "$tools" == "yes" ] # Download additional clang tools as requested
  then
    echo "Very sorry to disappoint you, but this is broken (just really, really broken)"
    echo "so you cannot have any cool extra clang tools today. Sorry."
    #cd clang/tools || error_report "Unable to change to clang/tools directory"
    #curl -L "$CLANG_URL"> extra.tar.xz || error_report "Unable to curl at $CLANG_URL"
    #temp_extra_name=`tar -tf extra.tar.xz | head -1 | cut -f1 -d "/"` || error_report "Can't find extra.tar.xz subdir name"
    #tar -xf extra.tar.xz || error_report "Unable to untar extra.tar.xz"
    #mv "$temp_extra_name" extra || error_report "Unable to rename $temp_extra_name extra"
  fi
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # We clone the software and return to our initial working directory

  # Build clang and llvm.
  echo "Building clang and llvm"
  mkdir "$install_dir"/build || error_report "Can't create $install_dir/clang-llvm/build"
  cd "$install_dir"/build || error_report "Can't change to build directory"
  cmake -G "Unix Makefiles" ../llvm || error_report "CMakeError building clang/llvm."
  make || error_report "Make error"
  if [ "$install" == "yes" ]  # Attempted installation requested. Run make install.
  then
    echo "Attempting to install llvm and clang"
    make install || error_report "Unable to install clang and llvm."
  fi

  # Make h2m, knowing the location of the installations' cmake configuration files
  # are stored in certain directories by default and the paths are easily specified.
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # Attempt to make the software. Note that cmake probably provides enough error reporting.
  # The most likely problem would be with a change in installation location with a later clang/llvm release
  # so that the cmake config files are no longer in the directories specified here.
  echo "Attempting to build h2m using default cmake configuration file locations"
  # This variable keeps track of whether the build directory reference is relative
  absolute=`is_absolute "$install_dir"`
  if [ ! "$absolute" ]  # If we built in a relative direcotry, we have a relative path.
  then
    install_dir="$start_dir"/"$install_dir"  # Now we have an absolute path which Cmake needs!
  fi
  cmake . -DClang_DIR="$install_dir"/build/lib/cmake/clang -DLLVM_DIR="$install_dir"/build/lib/cmake/llvm
  make  || exit 1
  exit 0
fi
# Installation and configuration are finished if a download was requested... otherwise...


# Request information about installation paths from the user. Information
# about LLVM and Clang installations is necessary if they are not in default
# locations where cmake can easily find them.
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
    echo "Otherwise library and include paths are needed."
    echo "Note that if a cmake configuration file was not provided"
    echo "for LLVM, this is almost certainly necessary."
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
    echo "Specify path to LLVM include files. See README.txt for details."
    read LLVM_INCLUDE_PATH
    echo "Specify path to LLVM library files. See README.txt for details."
    read LLVM_LIB_PATH
  fi
  if [ "$use_clang_config" == "y" ]  # Obtain directory path to ClangConfig.cmake
  then
    echo "Specify path to Clang cmake configuration file"
    read CLANG_DIR
  else  # Otherwise, obtain all the unpleasant paths we need for manual configuration.
    echo "Specify path to Clang library files. See README.txt for details."
    read CLANG_LIB_PATH
    echo "Specify path to Clang include files. See README.txt for details."
    read CLANG_INCLUDE_PATH
    echo "Specify path to Clang build files. See README.txt for details."
    read CLANG_BUILD_PATH
  fi
fi  # End interactive section

# The cmake_command will be built up piece by piece according to requested options
cmake_command=
# The User can specify the path to the LLVMConfig.cmake file and
# this provides CMake all informaiton necessary to include LLVM headers
# and link to LLVM library files.
if [ "$LLVM_DIR" ]  # Include LLVM location specs in the command
then
  echo "Configuring cmake LLVM information:"
  # CMake may complain about relative paths, so they are made absoulte here
  is_absolute "$LLVM_DIR" || LLVM_DIR="$start_dir/$LLVM_DIR"
  echo "LLVM_DIR=$LLVM_DIR"
  cmake_command="$cmake_command -DLLVM_DIR=$LLVM_DIR"
fi
# The User can specify the path to the ClangConfig.cmake file and this
# provides CMake all information necessary to include Clang headers
# and link to clang libraries
if [ "$CLANG_DIR" ]  # Include Clang location specs in the command
then
  echo "Configuring cmake Clang information:"
  # CMake many complain about relative paths, so they are made absoulte here
  is_absolute "$CLANG_DIR" || CLANG_DIR="$start_dir/$CLANG_DIR"
  echo "CLANG_DIR=$CLANG_DIR"
  cmake_command="$cmake_command -DClang_DIR=$CLANG_DIR"
fi

# Users can manually specify paths to include and library files for LLVM which CMake
# can pass to the compiler and linker to create the h2m tool.
if [ "$LLVM_LIB_PATH" ] && [ "$LLVM_INCLUDE_PATH" ]  # Check for all manual LLVM specs
then
  echo "Manually configuring llvm installation information:"
  # CMake sometimes complains about relative paths, so it is a good idea to 
  # prepend all relative paths to make them absolute
  is_absolute "$LLVM_LIB_PATH" || LLVM_LIB_PATH="$start_dir/$LLVM_LIB_PATH"
  is_absolute "$LLVM_INCLUDE_PATH" || LLVM_INCLUDE_PATH="$start_dir/$LLVM_INCLUDE_PATH"
  echo "LLVM_LIB_PATH=$LLVM_LIB_PATH, LLVM_INCLUDE_PATH=$LLVM_INCLUDE_PATH"
  cmake_command="$cmake_command -DLLVM_INCLUDE_PATH=$LLVM_INCLUDE_PATH -DLLVM_LIB_PATH=$LLVM_LIB_PATH"
elif [ "$LLVM_LIB_PATH" ] || [ "$LLVM_INCLUDE_PATH" ]
then  # Either both or neither must be set. Default locations to search cannot be reasonably guessed.
  error_report "Invalid options. Both LLVM_INCLUDE_PATH and LLVM_LIB_PATH are needed"
fi

# Users can manually specify paths to include and library files for Clang which CMake
# can pass to the compiler and linker to create the h2m tool.
# Include manual clang paths and check to make sure options have been passed properly
if [ "$CLANG_LIB_PATH" ] && [ "$CLANG_INCLUDE_PATH" ] && [ "$CLANG_BUILD_PATH" ]
then
  echo "Manually configuring clang installation information:"
  echo "CLANG_LIB_PATH=$CLANG_LIB_PATH, CLANG_INCLUDE_PATH=$CLANG_INCLUDE_PATH,"
  echo "CLANG_BUILD_PATH=$CLANG_BUILD_PATH"
  # CMake sometimes complains about relative paths, so all relative paths
  # are prepended here in order to guarantee absolute paths.
  is_absolute "$CLANG_INCLUDE_PATH" || CLANG_INCLUDE_PATH="$start_dir/$CLANG_INCLUDE_PATH"
  is_absolute "$CLANG_LIB_PATH" || CLANG_LIB_PATH="$start_dir/$CLANG_LIB_PATH"
  is_absolute "$CLANG_BUILD_PATH" || CLANG_BUILD_PATH="$start_dir/$CLANG_BUILD_PATH"
  cmake_command="$cmake_command -DCLANG_INCLUDE_PATH=$CLANG_INCLUDE_PATH"
  cmake_command="$cmake_command -DCLANG_LIB_PATH=$CLANG_LIB_PATH"
  cmake_command="$cmake_command -DCLANG_BUILD_PATH=$CLANG_BUILD_PATH"
# We need all three or none set. Default locations cannot be reasonably guessed.
elif [ "$CLANG_LIB_PATH" ] || [ "$CLANG_INCLUDE_PATH" ] || [ "$CLANG_BUILD_PATH" ]
then
  error_report "Invalid options. CLANG_LIB_PATH, CLANG_INCLUDE_PATH, and CLANG_BUILD_PATH are all needed"
elif [ "$CLANG_DIR_PATH" ] && [ ! "$LLVM_DIR_PATH" ]  # This is probably a fatal mistake.
then
  echo "Warning: the Clang package cannot be located without LLVM package information.">&2
  echo "If CMake is unable to find LLVM, it will not be able to find Clang.">&2
  echo "If errors occur, make sure to either specify LLVM's directory, or provide">&2
  echo "Clang's include and library path information manually">&2
fi

# Attempt to execute the cmake commands to create h2m
echo "cmake command is $cmake_command"  # Debugging line

# The echo use is necessary to keep cmake from interpretting everything as the source directory
cmake . `echo "$cmake_command" ` || exit 1
make || exit 1

exit 0
