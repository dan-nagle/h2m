#!/bin/sh
# Bash is the recommended shell. Ksh and zsh will also 
# probably work, as may dash.
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
  echo "-i interactive. All variables specified interactively"
  echo "-download_cmake downloads and installs cmake"
  echo "-download_cmake_dir gives the path to the cmakd download directory"
  echo "-download downloads llvm and clang"
  echo "-download_dir gives the path to a download location for llvm and clang"
  echo "-LLVM_DIR gives the path to an existing LLVM cmake config file"
  echo "-CLANG_DIR gives the path to an existing Clang cmake config file"
  echo "-LLVM_LIB_PATH gives the path to existing LLVM library files"
  echo "-LLVM_INCLUDE_PATH gives the path to existing LLVM header files"
  echo "-CLANG_LIB_PATH gives the path to existing Clang library files"
  echo "-CLANG_BUILD_PATH gives the path to an existing Clang build directory"
  echo "-CLANG_INCLUDE_PATH gives the path to existing Clang header files"
  echo "-install requests attempted installation of the software llvm and clang"
  echo "-install_h2m requests attempted installation of the h2m software"
  echo "-INSTALL_H2M_DIR gives the path to the h2m installation location"
  echo "-LLVM_URL gives an alternate URL from which to download LLVM"
  echo "-CLANG_URL gives an alternate URL from which to download Clang"
  echo "-CMAKE_URL gives an alternate URL from which to download CMake"
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
download_dir="./clang-llvm"  # Default installation directory is ./clang-llvm
download_cmake="no"
download_cmake_dir="./cmake_dir"
install_cmake_dir="/usr/local/bin"
CMAKE_URL="https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz"  # Default cmake url
LLVM_DIR=
CLANG_DIR=
LLVM_LIB_PATH=
LLVM_INCLUDE_PATH=
CLANG_LIB_PATH=
CLANG_BUILD_PATH=
CLANG_INCLUDE_PATH=
install="no"
interactive="no"
LLVM_URL="http://releases.llvm.org/4.0.0/llvm-4.0.0.src.tar.xz"
CLANG_URL="http://releases.llvm.org/4.0.0/cfe-4.0.0.src.tar.xz"
install_h2m="no"
INSTALL_H2M_DIR="/usr/local/bin"



# Process command line args via shift in a while loop
while [ $# -gt 0 ]
do
  case "$1" in
    -help) print_help 0;;  # Print help information and exit
    -i) interactive="yes";;  # Obtain options interactively.
    -download) download=yes;;  # A download is requested
    -download_dir) download_dir="$2"; shift;;  # Location to write clang/llvm
    -LLVM_DIR) LLVM_DIR="$2"; shift;;  # Location of existing llvm information
    -CLANG_DIR) CLANG_DIR="$2"; shift;;  # Location of existing clang information
    -LLVM_LIB_PATH) LLVM_LIB_PATH="$2"; shift;;  # Path to LLVM library in existence
    -LLVM_INCLUDE_PATH) LLVM_INCLUDE_PATH="$2"; shift;;  # Path to LLVM include in existence
    -CLANG_LIB_PATH) CLANG_LIB_PATH="$2"; shift;;  # Path to existing Clang library
    -CLANG_BUILD_PATH) CLANG_BUILD_PATH="$2"; shift;;  # Path to existing Clang build files
    -CLANG_INCLUDE_PATH) CLANG_INCLUDE_PATH="$2"; shift;;  # Path to existing Clang include files
    -install) install=yes;;  # Attempts an installation of Clang/LLVM
    -LLVM_URL) LLVM_URL="$2"; shift;;  # The overriding address of the LLVM source
    -CLANG_URL) CLANG_URL="$2"; shift;;  # The overriding address of the Clang source
    -CMAKE_URL) CMAKE_URL="$2"; shift;;   # The overriding CMake download URL
    -install_h2m) install_h2m="yes";;   # An h2m download is requested
    -INSTALL_H2M_DIR) INSTALL_H2M_DIR="$2"; shift;;  # Alternate installation directory
    -download_cmake) download_cmake="yes";;  # A CMake download is requested
    -download_cmake_dir) download_cmake_dir="$2"; shift;;  # The overriding cmake download directory
    *) echo "Invalid option, $1."; print_help 1;;  # Print help. Exit with error.
  esac
  shift
done

if [ ! -f "$PWD/h2m.cpp" ]  # If there is no h2m.cpp in this directory...
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
  echo "Do you need to download and install Cmake?"
  while [ "$download_cmake_temp" != "y" ] && [ "$download_cmake_temp" != "n" ]
  do
    echo "y or n required"
    read download_cmake_temp
  done
  echo "Do you need to download and install llvm and clang?"
  while [ "$download_install_temp" != "y" ] && [ "$download_install_temp" != "n" ]
  do
    echo "y or n required"
    read download_install_temp
  done
  # Acquire the needed information to download CMake and install it
  if [ "$download_cmake_temp" == "y" ]
  then
    download_cmake="yes";
    echo "Download directory for cmake?"
    read download_cmake_dir
    echo "Do you need to specify a non-standard URL for a the CMake download?"
    echo "Unless you know for a fact that Unix CMake 3.8.2 does not work"
    echo "on your system, you probably do not."
    while [ "$need_url" != "y" ] && [ "$need_url" != "n" ]
    do
      echo "y or n required"
      read need_url
    done
    if [ "$need_url" == "y" ]  # CMake is to be downloaded from a non-standard URL
    then
      echo "URL from which to download a CMake tar file?"
      read CMAKE_URL
    fi
  fi
  # Obtain information about downloading and/or installinc LLVM and Clang
  if [ "$download_install_temp" == "y" ]  # Obtain extra installation options
  then
    download=yes  # An actual download has been requested

    # Obtain the download directory
    echo "Where would you like to download clang and llvm?"
    echo "This makes little difference. ./clang-llvm is the default."
    read download_dir_temp

    # Attempts at installation for LLVM and Clang are made upon request.
    echo "Would you like to attempt installation of Clang and LLVM (run "make install")?"
    while [ "$install_attempt" != "y" ] && [ "$install_attempt" != "n" ]
    do
      echo "y or n required"
      read install_attempt
    done

    # Get URL information. Do we download from default sites or not?
    echo "Do you need to specify non-default download URLs?"
    echo "Unless you know Unix LLVM 4.0 will not work for you, you probably do not."
    while [ "$specify_urls" != "y" ] && [ "$specify_urls" != "n" ]
    do
      echo "y or n required"
      read specify_urls
    done
    if [ "$specify_urls" == "y" ]  # Obtain the special URLs as needed
    then
      echo "Please provide the URL from which to download the LLVM tar file."
      read LLVM_URL
      echo "Please provide the URL from which to download the Clang tar file."
      read CLANG_URL
    fi

    echo "Would you like to install h2m?"
    while [ "$h2m_install_temp" != "y" ] && [ "$h2m_install_temp" != "n" ]
    do
      echo "y/n required"
      read h2m_install_temp
    done   # End aquisition of information about what paths to obtain

    # Sort the obtained input into usable options for installation
    # including the download tool, download directory, and installation
    # preference.
    if [ "$download_dir_temp" == "" ]  # The user just hit enter, so use the default
    then
      echo "Defaulting to ./clang-llvm for download directory"
      download_dir="./clang-llvm"
    else
      download_dir="$download_dir_temp"  # The given download directory is set
    fi
    if [ "$h2m_install_temp" == "y" ] 
    then
      install_h2m="yes"  # We will attempt to install h2m as requested
      echo "Where would you like to install h2m?"
      echo "Recommended default is /usr/local/bin."
      read INSTALL_H2M_DIR
    fi 

    if [ "$install_attempt" == "y" ]  # Whether to run LLVM's make install or not
    then
       install="yes"
    fi
  fi  # End obtaining options for the download location and content
fi  # End interactive processing

# We start in this directory. We keep track of it for future reference.
start_dir="$PWD"

if [ "$download_cmake" == "yes" ]  # Commence with the download of CMake
then
  echo "Begining download of CMake to $download_cmake_dir"
  # Autodetect download tool
  which which>/dev/null || error_report "Command 'which' not found. Autodetection of curl/wget failed."
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
  
  if [ ! -d "$download_dir" ] # Check for existence of the download directory, create if needed
  then
    # Don't rely on the -p option unless you have to because it may not be available.
    mkdir "$download_cmake_dir" >/dev/null || mkdir -p "$download_cmake_dir" || error_report "Can't create $download_cmake_dir"
  fi 
  cd "$download_cmake_dir"

  # Obtain CMake source code form the internet with Wget or Curl as found
  if [ "$curl" == "yes" ]
  then
    curl -L "$CMAKE_URL" > cmake.tar.gz || error_report "Unable to curl at CMake at $CMAKE_URL"
  else
    wget -O cmake.tar.gz "$CMAKE_URL" || error_report "Unable to wget at CMake at $CMAKE_URL" 
  fi
  
  # Filter out the name of the main folder inside the tar directory
  temp_cmake_name=`tar -tf cmake.tar.gz | head -1 | cut -f1 -d "/"` || error_report "Can't find cmake.tar.gz subdir name"
  tar -xf cmake.tar.gz || error_report "Unable to untar cmake.tar.gz"
  mv "$temp_cmake_name" cmake || error_report "Cant rename $temp_cmake_name to cmake"
  cd cmake || error_report "Can't change to cmake directory"
  ./bootstrap || error_report "Unable to run bootstrap script in cmake directory."
  make || exit 1  # Make will provide enough error reporting. A failure here should be fatal.
  make install || exit 1 
  cd "$start_dir" || error_report "Unable to change back to $start_dir"
fi


# If there is a requested download, commence!
if [ "$download"  == "yes" ]
then
  echo "Beginning download to $download_dir of clang and llvm"
  # Autodetect download tool
  which which>/dev/null || error_report "Command 'which' not found. Autodetection of curl/wget failed."
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
  if [ ! -d "$download_dir" ]  # Checks to make sure the directory doesn't exit before creating it.
  then
    mkdir "$download_dir">/dev/null || mkdir -p "$download_dir" || error_report "Can't create $download_dir"
  fi
  cd "$download_dir" || error_report "Can't change to $download_dir"
  echo "Downloading LLVM from $LLVM_URL to $download_dir/llvm.tar.xz"
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
  echo "Downloading Clang from $CLANG_URL to $download_dir/llvm/tools/clang.tar"
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
  # This never worked and is currently not to be supported
  # if [ "$tools" == "yes" ] # Download additional clang tools as requested
  # then
  #  echo "Very sorry to disappoint you, but this is broken (just really, really broken)"
  #  echo "so you cannot have any cool extra clang tools today. Sorry."
  #  #cd clang/tools || error_report "Unable to change to clang/tools directory"
  #  #curl -L "$CLANG_URL"> extra.tar.xz || error_report "Unable to curl at $CLANG_URL"
  #  #temp_extra_name=`tar -tf extra.tar.xz | head -1 | cut -f1 -d "/"` || error_report "Can't find extra.tar.xz subdir name"
  #  #tar -xf extra.tar.xz || error_report "Unable to untar extra.tar.xz"
  #  #mv "$temp_extra_name" extra || error_report "Unable to rename $temp_extra_name extra"
  # fi
  cd "$start_dir" || error_report "Can't change to $start_dir"
  # We download the software and return to our initial working directory

  # Build clang and llvm.
  echo "Building clang and llvm"
  mkdir "$download_dir"/build || error_report "Can't create $download_dir/clang-llvm/build"
  cd "$download_dir"/build || error_report "Can't change to build directory"
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
  absolute=`is_absolute "$download_dir"`
  if [ ! "$absolute" ]  # If we built in a relative direcotry, we have a relative path.
  then
    download_dir="$start_dir"/"$download_dir"  # Now we have an absolute path which Cmake needs!
  fi
 
  # To provide more descriptive errors, a specific test for CMake is made.
  which cmake>/dev/null || error_report "Error: CMake is needed to build LLVM/Clang and h2m."

  if [ "$install_h2m" == "yes" ]  # If installation is requested, we must follow a different command 
  then
    cmake . -DClang_DIR="$download_dir"/build/lib/cmake/clang -DLLVM_DIR="$download_dir"/build/lib/cmake/llvm -DINSTALL_PATH="$INSTALL_H2M_DIR"
  else 
    cmake . -DClang_DIR="$download_dir"/build/lib/cmake/clang -DLLVM_DIR="$download_dir"/build/lib/cmake/llvm 
  fi
  make  || exit 1  # Either create the software or die trying
  if [ "$install_h2m" == "yes" ]   # If requested, attempt to install h2m
  then
    make install
  fi
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
    echo "Otherwise library and include paths are needed."
    while [ "$use_llvm_config" != "y" ] && [ "$use_llvm_config" != "n" ]
    do
      echo "y/n required"
      read use_llvm_config
    done
    echo "Specify paths to cmake configuration files for Clang?"
    echo "Otherwise library and include paths are needed."
    echo "Note that if a cmake configuration file was not provided"
    echo "for LLVM, a cmake configuration file will not be accepted"
    echo "for Clang because Clang depends on LLVM."
    while [ "$use_clang_config" != "y" ] && [ "$use_clang_config" != "n" ]
    do
      echo "y/n required"
      read use_clang_config
    done
  fi
  echo "Would you like to install h2m?"
  while [ "$h2m_install_temp" != "y" ] && [ "$h2m_install_temp" != "n" ]
  do
    echo "y/n required"
    read h2m_install_temp
  done   # End aquisition of information about what paths to obtain

  if [ "$use_llvm_config" == "y" ]  # Obtain directory path to LLVMConfig.cmake file
  then
    echo "Please specify the path to LLVM configuration file (llvm-config.cmake)."
    read LLVM_DIR
  elif [ "$use_llvm_config" == "n" ]  # Obtain unpleasant directory information
  then
    echo "Please specify path to LLVM include files. See README.txt for details."
    read LLVM_INCLUDE_PATH
    echo "Please specify path to LLVM library files. See README.txt for details."
    read LLVM_LIB_PATH
  fi
  if [ "$use_clang_config" == "y" ]  # Obtain directory path to ClangConfig.cmake
  then
    echo "Please specify path to Clang cmake configuration file (clang-config.cmake)"
    read CLANG_DIR
  elif [ "$use_clang_config" == "n" ] # Otherwise, obtain all the unpleasant paths we need for manual configuration.
  then
    echo "Please specify path to Clang library files. See README.txt for details."
    read CLANG_LIB_PATH
    echo "Plese specify path to Clang include files. See README.txt for details."
    read CLANG_INCLUDE_PATH
    echo "Please specify path to Clang build files. See README.txt for details."
    read CLANG_BUILD_PATH
  fi
  if [ "$h2m_install_temp" == "y" ]  # Obtain directory path for installation
  then
    install_h2m="yes" 
    echo "Please specify path to the installation directory for the h2m binary."
    echo "The recommended default is /usr/local/bin."
    read INSTALL_H2M_DIR_TEMP
    if [ "$INSTALL_H2M_DIR_TEMP" == "" ]  # The user just hit enter, so use the default
    then
      INSTALL_H2M_DIR="/usr/local/bin"
    else  # Use the specified directory
      INSTALL_H2M_DIR="$INSTALL_H2M_DIR_TEMP"
    fi
  fi
fi  # End interactive section

# The cmake_command will be built up piece by piece according to requested options
cmake_command=
# The User can specify the path to the LLVMConfig.cmake file and
# this provides CMake all information necessary to include LLVM headers
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
# can pass to the compiler and linker to create the h2m tool. This is a terrible idea.
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
  echo "Warning: the Clang usually cannot be located without LLVM package information.">&2
  echo "If CMake is unable to find LLVM, it will not be able to find Clang.">&2
  echo "If errors occur, make sure to either specify LLVM's directory, or provide">&2
  echo "Clang's include and library path information manually.">&2
fi

# Append on the installation directory if it was passed in
if [ "$install_h2m" == "yes" ] 
then
  cmake_command="$cmake_command -DINSTALL_PATH=$INSTALL_H2M_DIR" 
fi

# To provide more descriptive errors, a specific test for CMake is made.
which cmake>/dev/null || error_report "Error: CMake is needed to build LLVM/Clang and h2m."

# Attempt to execute the cmake commands to create h2m
# The echo use is necessary to keep cmake from interpretting everything as the source directory
cmake . `echo "$cmake_command" ` || exit 1
make || exit 1

# Install h2m if the request has been made
if [ "$install_h2m" == "yes" ]
then
  make install || exit 1
fi

exit 0
