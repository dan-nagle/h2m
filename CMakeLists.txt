# CMakeLists build file for the h2m autofortran tool.
# If at all possible, paths to the LLVMConfig.cmake and ClangConfig.cmake
# files should be specified. If this is not possible, or they cannot be
# found, there is the option to specify three directories for clang and two for
# llvm in order to manually locate header and include files.
# IMPORTANT NOTE: PATH EXPANSION (~ expansion) DOES NOT OCCUR. USE FULLY
# QUALIFIED PATHS (ie /Users/manderso/clang-llvm/llvm/tools/clang/include)
# For Clang:
# speficy -DCLANG_INCLUDE_DIRS on the command line to be the directory containing
# ./clang/Frontend/FrontendAction.h (usually llvm/tools/clang/include)
# specify -DCLANG_BUILD_PATH on the command line to be the directory containing
# ./clang/AST/DeclNodes.inc (usually /build/tools/clang/include
# specify -DCLANG_LIB_PATH on the command line to be the directory containing
# all .a/.so files for clang. This should be in /build/lib
# In case of linking errors complaining of something undefined for
# the hardware, try specifying -DALL=ON to link to all LLVM lib files.


cmake_minimum_required(VERSION 3.2)
project(h2m)

# This is the option to link in all LLVM libraries in case of emergency.
option(ALL "ALL" OFF)


# Find the package for LLVM or else obtain paths to includes and library files
find_package(LLVM CONFIG)
if(LLVM_FOUND) # We found the config and can use its definitions
  message(STATUS "Found LLVM as ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  add_definitions(${LLVM_DEFINITIONS})
  include_directories(${LLVM_INCLUDE_DIRS})
  #DEBUGLINE message(STATUS "LLVM include directories are ${LLVM_INCLUDE_DIRS}")
  
  if(${ALL}) # An option to include all libraries in the build.
    message(STATUS "Linking all LLVM libraries, as requested.")
    llvm_map_components_to_libnames(llvm_libs all)
  else()
    llvm_map_components_to_libnames(llvm_libs support core irreader)
  endif()
elseif((EXISTS ${LLVM_LIB_PATH}) AND (EXISTS ${LLVM_INCLUDE_PATH})) # Woe, all must be done by hand
  message(STATUS "Found LLVM directory information")
  message(STATUS "Be aware that Clang directory information must also be specified manually")
  message(STATUS "because automatic location of the Clang package depends on the LLVM package.")
  message(STATUS "Using LLVM libs in ${LLVM_LIB_PATH}")
  message(STATUS "USING LLVM includes in ${LLVM_INCLUDE_PATH}")

  # These are a best guess at the needed definitions. They are meant for version 4.0.
  add_definitions(-DLLVM_BUILD_GLOBAL_ISEL -D_DEBUG -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS)
  include_directories(${LLVM_INCLUDE_PATH})
  # These are all the libraries that could possibly be needed. Chances are only a few actually are needed
  # but without the LLVM config file I have no idea.
  set(LLVM_include_libs LLVMLTO LLVMPasses LLVMObjCARCOpts LLVMMIRParser LLVMSymbolize LLVMDebugInfoPDB
     LLVMDebugInfoDWARF LLVMCoverage LLVMTableGen LLVMOrcJIT LLVMXCoreDisassembler LLVMXCoreCodeGen 
     LLVMXCoreDesc LLVMXCoreInfo LLVMXCoreAsmPrinter LLVMSystemZDisassembler LLVMSystemZCodeGen 
     LLVMSystemZAsmParser LLVMSystemZDesc LLVMSystemZInfo LLVMSystemZAsmPrinter LLVMSparcDisassembler 
     LLVMSparcCodeGen LLVMSparcAsmParser LLVMSparcDesc LLVMSparcInfo LLVMSparcAsmPrinter LLVMRISCVDesc 
     LLVMRISCVCodeGen LLVMRISCVInfo LLVMPowerPCDisassembler LLVMPowerPCCodeGen LLVMPowerPCAsmParser 
     LLVMPowerPCDesc LLVMPowerPCInfo LLVMPowerPCAsmPrinter LLVMNVPTXCodeGen LLVMNVPTXDesc LLVMNVPTXInfo 
     LLVMNVPTXAsmPrinter LLVMMSP430CodeGen LLVMMSP430Desc LLVMMSP430Info LLVMMSP430AsmPrinter 
     LLVMMipsDisassembler LLVMMipsCodeGen LLVMMipsAsmParser LLVMMipsDesc LLVMMipsInfo LLVMMipsAsmPrinter 
     LLVMLanaiDisassembler LLVMLanaiCodeGen LLVMLanaiAsmParser LLVMLanaiDesc LLVMLanaiAsmPrinter 
     LLVMLanaiInfo LLVMHexagonDisassembler LLVMHexagonCodeGen LLVMHexagonAsmParser LLVMHexagonDesc 
     LLVMHexagonInfo LLVMBPFDisassembler LLVMBPFCodeGen LLVMBPFDesc LLVMBPFInfo LLVMBPFAsmPrinter 
     LLVMARMDisassembler LLVMARMCodeGen LLVMARMAsmParser LLVMARMDesc LLVMARMInfo LLVMARMAsmPrinter 
     LLVMAMDGPUDisassembler LLVMAMDGPUCodeGen LLVMAMDGPUAsmParser LLVMAMDGPUDesc LLVMAMDGPUInfo 
     LLVMAMDGPUAsmPrinter LLVMAMDGPUUtils LLVMAArch64Disassembler LLVMAArch64CodeGen LLVMAArch64AsmParser 
     LLVMAArch64Desc LLVMAArch64Info LLVMAArch64AsmPrinter LLVMAArch64Utils LLVMObjectYAML LLVMLibDriver 
     LLVMOption LLVMX86Disassembler LLVMX86AsmParser LLVMX86CodeGen LLVMGlobalISel LLVMSelectionDAG 
     LLVMAsmPrinter LLVMDebugInfoCodeView LLVMDebugInfoMSF LLVMX86Desc LLVMMCDisassembler LLVMX86Info 
     LLVMX86AsmPrinter LLVMX86Utils LLVMMCJIT LLVMLineEditor LLVMInterpreter LLVMExecutionEngine 
     LLVMRuntimeDyld LLVMCodeGen LLVMTarget LLVMCoroutines LLVMipo LLVMInstrumentation libLLVMVectorize 
     LLVMScalarOpts LLVMLinker LLVMIRReader LLVMAsmParser LLVMInstCombine LLVMTransformUtils LLVMBitWriter 
     LLVMAnalysis LLVMObject LLVMMCParser LLVMMC LLVMBitReader LLVMProfileData LLVMCore LLVMSupport 
     LLVMDemangle)
     # Manually link in required system libraries
     SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lcurses -lz -lm")
else()
  message(STATUS "Failed fo find LLVMConfig.cmake")
  message(STATUS "Valid LLVM_LIB_PATH and LLVM_INCLUDE_PATH not found")
  message(STATUS "Specify path to LLVMConfig.cmake as -DLLVM_DIR=")
  message(STATUS "Otherwise specify -DLLVM_LIB_PATH and -DLLVM_INCLUDE_PATH")
  message(FATAL_ERROR "LLVM not found and valid paths not specified")
endif()

# Find the package for clang or else obtain paths to includes and library files
find_package(Clang CONFIG)
if(Clang_FOUND) # We found the config and can use its definitions
  message(STATUS "Found Clang")
  message(STATUS "Proceeding from ClangConfig.cmake")

  # A bug in one of the releases means that the directories might need manual specification
  # Two different directory paths are needed to find all the clang includes.
  find_path(AUX_CLANG_DIR_H NAMES ../tools/clang/include/clang/Basic/Version.h HINTS ${LLVM_INCLUDE_DIRS})
  if(IS_DIRECTORY "${AUX_CLANG_DIR_H}") 
    include_directories("${AUX_CLANG_DIR_H}/../tools/clang/include")
    #DEBUGLINE message(STATUS "Including ${AUX_CLANG_DIR_H}/../tools/clang/include")
  endif()
  find_path(AUX_CLANG_DIR_INC NAMES ../tools/clang/include/clang/Basic/Version.inc HINTS ${LLVM_INCLUDE_DIRS})
  if(IS_DIRECTORY "${AUX_CLANG_DIR_INC}")
    include_directories("${AUX_CLANG_DIR_INC}/../tools/clang/include")
    #DEBUGLINE message(STATUS "Including ${AUX_CLANG_DIR_INC}/../tools/clang/include")
  endif()
  #DEBUGLINE message(STATUS "Searched for ../tools/clang/include/clang/Basic/Version.h in ${LLVM_INCLUDE_DIRS}")
  #DEBUGLINE message(STATUS "Searched for ../tools/clang/include/clang/Basic/Version.inc in ${LLVM_INCLUDE_DIRS}")

  # If the include directories are defined (as they should be!) use those.
  # Double includes are not an issue as far as I can tell. CMake doesn't care
  # if these directories are included twice.
  if(IS_DIRECTORY "${CLANG_INCLUDE_DIRS}")
    include_directories("${CLANG_INCLUDE_DIRS}")
    #DEBUGLINE message(STATUS "Including ${CLANG_INCLUDE_DIRS}")
  endif() 
  # The clang package will resolve these to their proper files without further intervention.
  set(clang_libs clangTooling clangBasic clangASTMatchers clangFrontend)
elseif((EXISTS ${CLANG_LIB_PATH}) AND (EXISTS ${CLANG_INCLUDE_PATH}) AND (EXISTS ${CLANG_BUILD_PATH})) # Woe, all by hand
  message(STATUS "Found Clang directory information")
  message(STATUS "Using clang libs in ${CLANG_LIB_PATH}")
  message(STATUS "Using clang includes in ${CLANG_INCLUDE_PATH}")
  message(STATUS "Using clang build library in ${CLANG_BUILD_PATH}")

  # Include files from the two paths as needed
  # Afterwards, seek out the libraries from the given path, searching for a variety of names
  include_directories(${CLANG_INCLUDE_PATH} ${CLANG_BUILD_PATH})
  # Search through the list and find all the libraries to link to; set them as clang_libs
  set(link_clang_libs clangTooling clangFrontendTool clangFrontend clangDriver clangSerialization clangCodeGen
    clangParse clangSema clangStaticAnalyzerFrontend clangStaticAnalyzerCheckers clangStaticAnalyzerCore
    clangAnalysis clangARCMigrate clangRewrite clangRewriteFrontend clangEdit clangAST clangLex clangBasic
    clangASTMatchers)

else() # This could be because paths were not given or are invalid
  message(STATUS "Failed to find ClangConfig.cmake")
  message(STATUS "Valid CLANG_LIB_PATH, CLANG_BUILD_PATH and CLANG_INCLUDE_PATH not found")
  message(STATUS "Specify path to ClangConfig.cmake as -DClang_DIR=")
  message(STATUS "Otherwise specify -DCLANG_LIB_PATH, -DCLANG_BUILD_PATH and -DCLANG_INCLUDE_PATH")
  message(FATAL_ERROR "Clang not found and valid paths not specified")
endif()

# All Clang projects MUST be compiled with hidden visibility and no rtti, the same way Clang is compiled.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden")

# get_property(included DIRECTORY PROPERTY INCLUDE_DIRECTORIES) # Debugging message to see included directories
# message(STATUS ${included})

# The h2m include files are located in this directory.
include_directories("./include")

# CMake will take care of local include files (and the non-locals now that they are specified)
add_executable(h2m src/h2m.cpp src/function_decl_formatter.cpp src/decl_formatters.cpp
    src/var_decl_formatter.cpp src/type_formatter.cpp src/macro_formatter.cpp)

# Find and map all the given clang libraries and link them to the executable
# Only invoked if the cmake.config file wasn't found
foreach(lib ${link_clang_libs})
  # Find library has special caching so this has to be done with double variables
  find_library(found_${lib} NAMES lib${lib}.a ${lib}.a lib${lib}.so ${lib}.so
    HINTS "${CLANG_LIB_PATH}")
  # Determine whether or not find_library was successful
  if((EXISTS ${found_${lib}}))
    message(STATUS "Including clang library ${found_${lib}}")
    # Include the library as UNKNOWN because it may be .a or .so
    add_library(${lib} UNKNOWN IMPORTED)
    set_property(TARGET ${lib} PROPERTY IMPORTED_LOCATION ${found_${lib}})
    target_link_libraries(h2m "${lib}")
  else()
    message(WARNING "Unable to locate clang library, ${lib}")  # Let people know what's wrong.
    message(STATUS "Searched ${CLANG_LIB_PATH}/lib${lib}.a")
  endif()
endforeach()

# Find and map all the given LLVM libraries and link them to the executable
# Only invoked if the cmake.config file wasn't found
foreach(llvm_lib ${LLVM_include_libs})
  # Find library has special caching so this has to be done with double variables
  find_library(found_${llvm_lib} NAMES lib${llvm_lib}.a ${llvm_lib}.a  lib${llvm_lib}.so ${llvm_lib}.so
    HINTS "${LLVM_LIB_PATH}")
  # Detemine whether or not find_library was successful 
  if((EXISTS "${found_${llvm_lib}}"))
    message(STATUS "Including llvm library ${found_${llvm_lib}}")
    # We do not know whether this is a .a or a .so file
    add_library(${llvm_lib} UNKNOWN IMPORTED)
    set_property(TARGET ${llvm_lib} PROPERTY IMPORTED_LOCATION ${found_${llvm_lib}})
    target_link_libraries(h2m "${llvm_lib}")
  else()
    message(WARNING "Unable to locate llvm library, ${llvm_lib}")
    message(STATUS "Searched ${LLVM_LIB_PATH}/lib${llvm_lib}.a")
  endif()
endforeach()

# Link the found libraries to the executable, clang libraries first due to dependencies
target_link_libraries(h2m ${clang_libs} ${llvm_libs})

# Create an installation target if a valid -DINSTALL_PATH was given
if((EXISTS "${INSTALL_PATH}"))
  message(STATUS "Installation requested to ${INSTALL_PATH}")
  install(TARGETS h2m RUNTIME DESTINATION "${INSTALL_PATH}")
else()
  message(STATUS "Valid installation path not found.")
  message(STATUS "No installation target will be created.")
endif()
