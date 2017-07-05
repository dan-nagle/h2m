// This is the source code of the h2m autofortran tool
// envisioned by Dan Nagle, written by Sisi Liu and revised
// by Michelle Anderson at NCAR.

#include "h2m.h"
//-----------formatter functions----------------------------------------------------------------------------------------------------

// A helper function to be used to output error line information
// If the location is invalid, it returns a message about that.
static void LineError(PresumedLoc sloc) {
  if (sloc.isValid()) {
    errs() << sloc.getFilename() << " " << sloc.getLine() << ":" << sloc.getColumn() << "\n";
  } else {
    errs() << "Invalid file location \n";
  }
}

// A helper function to be used to find lines/names which are too 
// long to be valid fortran. It returns the string which is passed
// in, regardless of the outcome of the test. The first argument
// is the string to be checked. The integer agument is the
// limit (how many characters are allowed), the boolean is whether
// or not to warn, and the Presumed location is for passing to 
// LineError if needed. 
static string CheckLength(string tocheck, int limit, bool no_warn, PresumedLoc sloc) {
  if (tocheck.length() > limit && no_warn == false) {
    errs() << "Warning: length of '" << tocheck;
    errs() << "'\n exceeds maximum. Fortran name and line lengths are limited.\n";
    LineError(sloc);
  }
  return tocheck;  // Send back the string!
}

// This function exists to make sure that a type is not declared
// twice. This frequently happens with typedefs renaming structs.
// This results in duplicate-name-declaration errors in Fortran
// because there are no seperate name-look-up procedures for
// "tag-names" as there are in C (ie in C typedef struct Point point
// is legal but in Fortran this causes conflicts.) A static map will
// make sure that no typedef declares an already declared name.
// If the name has already been seen, it returns false. If it hasn't,
// it adds it to a set (will return false if called again with that
// name) and returns true.
bool RecordDeclFormatter::StructAndTypedefGuard(string name) {
  static std::set<string> seennames;  // Records all identifiers seen.

  // Put the name into lowercase. Fortran is not case sensitive.
  std::locale location;  // Determines if a lowercase exists
  for (string::size_type j = 0; j < name.length(); j++) {
    name[j] = std::tolower(name[j], location); 
  }

  // If we have already added this file to the set... false
  if (seennames.find(name) != seennames.end()) {
    return false;
  } else {  // Otherwise, add it to the stack and return true
    seennames.insert(name);
    return true;
  }
}

// Fetches the fortran module name from a given filepath Filename
// IMPORTANT: ONLY call this ONCE for ANY FILE. The static 
// structure requires this, otherwise you will have all sorts of
// bizarre problems because it will think it has seen various module
// names multiple times and start appending numbers to their names.
// It keps track of modules seen in a static map and appends a number
// to the end of duplicates to create unique names. It also adds
// module_ to the front.
static string GetModuleName(string Filename, Arguments& args) {
  static std::map<string, int> repeats;
  size_t slashes = Filename.find_last_of("/\\");
  string filename = Filename.substr(slashes+1);
  size_t dot = filename.find('.');
  filename = filename.substr(0, dot);
  if (repeats.count(filename) > 0) {  // We have found a repeated module name
    int append = repeats[filename];
    repeats[filename] = repeats[filename] + 1;  // Record the new repeat in the map
    string oldfilename = filename;  // Used to report the duplicate, no other purpose
    filename = filename + "_" + std::to_string(append);  // Add _# to the end of the name
    // Note that module_ will be prepended to the name prior to returning the string.
    if (args.getSilent() == false) {
      errs() << "Warning: repeated module name module_" << oldfilename << " found, source is " << Filename;
      errs() << ". Name changed to module_" << filename << "\n";
    }
  } else {  // Record the first appearance of this module name
    repeats[filename] = 2;
  }
  filename = "module_" + filename;
  if (filename.length() > 63 && args.getSilent() == false) {
    errs() << "Warning: module name, " << filename << " too long.";
  }
  return filename;
}

// Command line options:

// Apply a custom category to all command-line options so that they are the
// only ones displayed. This avoids lots of clang options cluttering the
// help output.
static llvm::cl::OptionCategory h2mOpts("Options for the h2m translator");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nSee README.txt for more information on h2m behavior.\n");

// Positional parameter: the first input parameter should be the compilation file. 
// Currently, h2m is only designed to take one file at a time.
static cl::opt<string> SourcePaths(cl::Positional, cl::cat(h2mOpts), cl::desc("<source0>"));

// Output file, option is -out or -o.
static cl::opt<string> OutputFile("out", cl::init(""), cl::cat(h2mOpts), cl::desc("Output file"));
static cl::alias OutputFile2("o", cl::desc("Alias for -out"), cl::cat(h2mOpts), cl::aliasopt(OutputFile));

// Boolean option to recursively process includes. The default is not to recursively process.
static cl::opt<bool> Recursive("recursive", cl::cat(h2mOpts), cl::desc("Include other header files recursively via USE statements"));
static cl::alias Recrusive2("r", cl::desc("Alias for -recursive"), cl::cat(h2mOpts), cl::aliasopt(Recursive));

// Boolean option to silence less critical warnings (ie warnings about statements commented out)
static cl::opt<bool> Quiet("quiet", cl::cat(h2mOpts), cl::desc("Silence warnings about lines which have been commented out"));
static cl::alias Quiet2("q", cl::desc("Alias for -quiet"), cl::cat(h2mOpts), cl::aliasopt(Quiet));

// Boolean option to silence all warnings save those that are absolutely imperative (ie output failure)
static cl::opt<bool> Silent("silent", cl::cat(h2mOpts), cl::desc("Silence all tool warnings (Clang warnings will still appear)"));
static cl::alias Silent2("s", cl::cat(h2mOpts), cl::desc("Alias for -silent"), cl::aliasopt(Silent));

// Boolean option to ignore critical clang errors that would otherwise cause termination
// during preprocessing and to keep the output file despite any other problems.
static cl::opt<bool> Optimistic("keep-going", cl::cat(h2mOpts), cl::desc("Continue processing and keep output in spite of errors"));
static cl::alias Optimistic2("k", cl::desc("Alias for -keep-going"), cl::cat(h2mOpts), cl::aliasopt(Optimistic));

// Boolean option to ignore system header files when processing recursive includes. 
// The default is to include them.
static cl::opt<bool> NoHeaders("no-system-headers", cl::cat(h2mOpts), cl::desc("Do not recursively translate system header files"));
static cl::alias NoHeaders2("n", cl::desc("Alias for -no-system-headers"), cl::cat(h2mOpts), cl::aliasopt(NoHeaders));

static cl::opt<bool> IgnoreThis("ignore-this", cl::cat(h2mOpts), cl::desc("Do not translate this file, only its included headers"));
static cl::alias IgnoreThis2("i", cl::desc("Alias for -ignore-this"), cl::cat(h2mOpts), cl::aliasopt(IgnoreThis));

// Option to specify the compiler to use to test the output. No specification means no compilation.
static cl::opt<string> Compiler("compile", cl::cat(h2mOpts), cl::desc("Command to attempt compilation of the output file"));
static cl::alias Compiler2("c", cl::desc("Alias for -compile"), cl::cat(h2mOpts), cl::aliasopt(Compiler));

// Option to send all non-system header information for this file into a single Fortran module
static cl::opt<bool> Together("together", cl::cat(h2mOpts), cl::desc("Send this entire file and all non-system includes to one module"));
static cl::alias Together2("t", cl::cat(h2mOpts), cl::desc("Alias for -together"), cl::aliasopt(Together));

// Option to transpose arrays to match Fortran dimensions (C row major
// ordering is swapped for Fortran column major ordering).
static cl::opt<bool> Transpose("array-transpose", cl::cat(h2mOpts), cl::desc("Transpose array dimensions"));
static cl::alias Transpose2("a", cl::cat(h2mOpts), cl::desc("Alias for array-transpose"), cl::aliasopt(Transpose));

// These are the argunents for the clang compiler driver.
static cl::opt<string> other(cl::ConsumeAfter, cl::desc("Front end arguments"));



// -----------initializer RecordDeclFormatter--------------------
CToFTypeFormatter::CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc loc, Arguments &arg): ac(ac), args(arg) {
  c_qualType = qt;
  sloc = loc;
};

// Determines whether the qualified type offered is identical to that it is called on.
// Pointer types are only distinguished in terms of function vs data pointers. 
bool CToFTypeFormatter::isSameType(QualType qt2) {
  // for pointer type, only distinguish between the function pointer from other pointers
  if (c_qualType.getTypePtr()->isPointerType() and qt2.getTypePtr()->isPointerType()) {
    // True if both are function pointers
    if (c_qualType.getTypePtr()->isFunctionPointerType() and qt2.getTypePtr()->isFunctionPointerType()) {
      return true;
    // True if both are not functoin pointers
    } else if ((!c_qualType.getTypePtr()->isFunctionPointerType()) and (!qt2.getTypePtr()->isFunctionPointerType())) {
      return true;
    } else {
      return false;
    }
  } else {
    // No pointers involved. Use the overloaded operator ==
    return c_qualType == qt2;
  }
};

// Typically this returns the raw id, but in the case of an array, it must 
// determine an array suffix by determining the type of the array, the element
// size, and the length in order to provide a declaration.
string CToFTypeFormatter::getFortranIdASString(string raw_id) {
  // Determine if it needs to be substituted out
  if (c_qualType.getTypePtr()->isArrayType()) {
    // In this case, the array bounds are fixed and we can determine the
    // exact dimensions of the declared array.
    if (c_qualType.getTypePtr()->isConstantArrayType()) {
      QualType element_type = c_qualType;  // Element type of the array for looping.
      std::stack<string> dimensions;  // Holds dimensions so they can be reversed
      raw_id += "(";
      // Loop through the array's dimensions, pulling off layers by fetching
      // the element types and getting their array information.
      while (element_type.getTypePtr()->isConstantArrayType()) {
        const ConstantArrayType *cat = ac.getAsConstantArrayType(element_type);
        // Note that the getSize() function returns an arbitrary precision integer
        // and a toString method needs the radix (10) and signed status (true).
        // This if statement either prepares a stack to reverse the
        // dimensions of the array (if requested) or else adds them
        // into place without reversal
        if (args.getArrayTranspose() == true) {
          dimensions.push(cat->getSize().toString(10, true));
        } else {
          raw_id += cat->getSize().toString(10, true) + ", ";
        }
        element_type = cat->getElementType();
        cat = ac.getAsConstantArrayType(element_type);
      }
      // Build up the raw ID from the stack, reversing array dimensions      // if requested and the stack was built
      if (args.getArrayTranspose() == true) {
        while (dimensions.empty() == false) {
          raw_id += dimensions.top() + ", ";
          dimensions.pop(); 
        }
      }
      // Close the Fortran array declaration
      // First, we erase the extra space and comma from the last loop iteration
      raw_id.erase(raw_id.end() - 2, raw_id.end());
      raw_id += ")";
    } else {  // This is not necessarilly a constant length array.
      // Only single dimension arrays are handled correctly here.
      const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType ();
      int typeSize = ac.getTypeSizeInChars(c_qualType).getQuantity();
      int elementSize = ac.getTypeSizeInChars(e_qualType).getQuantity();
      int numOfEle = typeSize / elementSize;
      string arr_suffix = "(" + to_string(numOfEle) + ")";
      raw_id += arr_suffix;
    }
  }
  return raw_id;
};

// The type in C is found in the c_qualType variable. The type is 
// determined over a series of if statements and a suitable Fortran 
// equivalent is returned as a string. The boolean typeWrapper dictates
// whether or not the return should be of the form FORTRANTYPE(C_QUALIFIER)
// or just of the form C_QUALIFIER. True means it needs the FORTRANTYPE(C_QUALIFIER)
// format.
string CToFTypeFormatter::getFortranTypeASString(bool typeWrapper) {
  string f_type;

  // Handle character types
  if (c_qualType.getTypePtr()->isCharType()) {
    if (typeWrapper) {
      f_type = "CHARACTER(C_CHAR)";
    } else {
      f_type = "C_CHAR";
    }
  // Handle boolean types
  } else if (c_qualType.getTypePtr()->isBooleanType()) {
    if (typeWrapper) {
      f_type = "LOGICAL(C_BOOL)";
    } else {
      f_type = "C_BOOL";
    }
  // Handle integer types
  } else if (c_qualType.getTypePtr()->isIntegerType()) {
     // Match the necessary integer type with string matching because
     // different types may have the same size in bytes.
     // Handle a size_t
     if (c_qualType.getAsString()== "size_t") {
      if (typeWrapper) {
        f_type = "INTEGER(C_SIZE_T)";
      } else {
        f_type = "C_SIZE_T";
      }
    // Handle integers which are 'chars'
    } else if (c_qualType.getAsString()== "unsigned char" or c_qualType.getAsString()== "signed char") {
      if (typeWrapper) {
        f_type = "INTEGER(C_SIGNED_CHAR)";
      } else {
        f_type = "C_SIGNED_CHAR";
      }        
    // Handle a short type
    } else if (c_qualType.getAsString().find("short") != std::string::npos) {
      if (typeWrapper) {
        f_type = "INTEGER(C_SHORT)";
      } else {
        f_type = "C_SHORT";
      }  
    // Handle a long or a long long. Long longs are ignored here. The type is assumed to be long.
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "INTEGER(C_LONG)";
      } else {
        f_type = "C_LONG";
      }      
    // There are other types of ints, but a type of INT is assumed.
    } else {
      if (typeWrapper) {
        f_type = "INTEGER(C_INT)";
      } else {
        f_type = "C_INT";
      }      
    }

  // Handle the translation of all REAL types.
  } else if (c_qualType.getTypePtr()->isRealType()) {
    // Handle translation of a long double
    if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "REAL(C_LONG_DOUBLE)";
      } else {
        f_type = "C_LONG_DOUBLE";
      } 
    // Handle translation of a float
    } else if (c_qualType.getAsString()== "float") {
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT)";
      } else {
        f_type = "C_FLOAT";
      }
    // Handle translation of a float_128
    } else if (c_qualType.getAsString()== "__float128") {
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT128)";
      } else {
        f_type = "C_FLOAT128";
      }
    // Assume that this "other" kind of real is a double
    } else {
      if (typeWrapper) {
        f_type = "REAL(C_DOUBLE)";
      } else {
        f_type = "C_DOUBLE";
      }      
    }
  // Handle translation of a C derived complex type
  } else if (c_qualType.getTypePtr()->isComplexType ()) {
    // Handle translation of a complex float
    if (c_qualType.getAsString().find("float") != std::string::npos) {
      if (typeWrapper) {
        f_type = "COMPLEX(C_FLOAT_COMPLEX)";
      } else {
        f_type = "C_FLOAT_COMPLEX";
      }        
    // Handle translation of a complex long double
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "COMPLEX(C_LONG_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_LONG_DOUBLE_COMPLEX";
      }
    // Assume that this is a complex double type and handle it as such
    } else {
      if (typeWrapper) {
        f_type = "COMPLEX(C_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_DOUBLE_COMPLEX";
      }
    }
  // Translate a C pointer
  } else if (c_qualType.getTypePtr()->isPointerType ()) {
    // C function pointers are translated to a special type
    if (c_qualType.getTypePtr()->isFunctionPointerType()){
      if (typeWrapper) {
        f_type = "TYPE(C_FUNPTR)";
      } else {
        f_type = "C_FUNPTR";
      }
    // All other data pointers are translated here
    } else {
      if (typeWrapper) {
        f_type = "TYPE(C_PTR)";
      } else {
        f_type = "C_PTR";
      }
    }
  // Translate a structure declaration. We will recieve
  // the declaration in the form "struct structname" and 
  // will have to deal with the space between the words.
  // Unions have no interoperable Fortran incranation but they
  // are translated as TYPE's and are handled here as well.
  } else if (c_qualType.getTypePtr()->isStructureType() ||
	     c_qualType.getTypePtr()->isUnionType()) {
    f_type = c_qualType.getAsString();
    // We cannot have a space in a fortran type. Erase up
    // to the space.
    size_t found = f_type.find_first_of(" ");
    while (found != string::npos) {
      f_type.erase(0, found + 1);  // Erase up to the space
      found=f_type.find_first_of(" ");
    }
    if (f_type.front() == '_') {
      if (args.getSilent() == false) {
        errs() << "Warning: fortran names may not begin with an underscore.";
        errs() << f_type << " renamed " << "h2m" << f_type << "\n";
        LineError(sloc);
      }
      f_type = "h2m" + f_type;  // Prepend h2m to fix the naming problem
    }
    if (typeWrapper) {
      f_type = "TYPE(" + f_type + ")";
    } 
  // Handle an array type declaration
  } else if (c_qualType.getTypePtr()->isArrayType()) {
    const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
    QualType e_qualType = at->getElementType ();
    // Call this function again on the type found inside the array
    // declaration. This recursion will determine the correct type.
    CToFTypeFormatter etf(e_qualType, ac, sloc, args);
    f_type = etf.getFortranTypeASString(typeWrapper);
  // We do not know what the type is. We print a warning and special
  // text around the type in the output file.
  } else {
    f_type = "unrecognized_type(" + c_qualType.getAsString()+")";
    // Warning only in the case of a typewrapper avoids repetitive error messages
    if (typeWrapper) {
      if (args.getSilent() == false) {
        errs() << "Warning: unrecognized type (" << c_qualType.getAsString() << ")\n";
        LineError(sloc);
      }
    }
  }
  return f_type;
};


// Determines whether or not an input string resembles an integer.
// Useful in determining how to translate a C macro because macros
// do not have types. It looks for pure numbers and for various
// type/format specifiers that might be present in an "int."
bool CToFTypeFormatter::isIntLike(const string input) {
  // "123L" "18446744073709551615ULL" "18446744073709551615UL" 
  if (std::all_of(input.begin(), input.end(), ::isdigit)) {
    return true;
  } else {
    string temp = input;
    size_t doubleF = temp.find_first_of(".eF");
    if (doubleF != std::string::npos) {
      return false;
    }    
    
    // If there are no digits, it is not a number.
    size_t found = temp.find_first_of("01234567890");
    if (found==std::string::npos) {
      return false;
    }

    while (found != std::string::npos)
    {
      temp.erase(found, found+1);
      found=temp.find_first_of("01234567890");
    }
    
    if (!temp.empty()) {
      found = temp.find_first_of("xUL()- ");
      while (found != std::string::npos)
      {
        temp.erase(found, found+1);
        found=temp.find_first_of("xUL()- ");
      }
      return temp.empty();
    } else {
      return false;
    }
  }
};

// Determines whether or not the string input resembles a
// double. Useful for determining how to translate a C macro
// which doesn't have a type. It looks for a decimal place
// and digits and also handles various letters that might
// indicate type/format of a double-like entity.
bool CToFTypeFormatter::isDoubleLike(const string input) {
  // "1.23", 1.18973149535723176502e+4932L
  string temp = input;
  size_t found = temp.find_first_of("01234567890");
  if (found==std::string::npos) {
    return false;
  }
  while (found != std::string::npos)
  {
    temp.erase(found, found+1);
    found=temp.find_first_of("01234567890");
  }
  // no digit anymore
  if (!temp.empty()) {
    size_t doubleF = temp.find_first_of(".eFUL()+- ");
    while (doubleF != std::string::npos)
    {
      temp.erase(doubleF, doubleF+1);
      doubleF=temp.find_first_of(".eFUL()+- ");
    }
    return temp.empty();
  } else {
    return false;
  }
  
  if (!temp.empty()) {
    found = temp.find_first_of(".eFL()- ");
    while (found != std::string::npos)
    {
      temp.erase(found, found+1);
      found=temp.find_first_of("xUL()- ");
    }
    return temp.empty();
  } else {
    return false;
  }
};

// Determines whether or not an input string can be
// treated as a string constant ("abc"). Useful 
// for determining how to translate a C macro
// which has no type.
bool CToFTypeFormatter::isString(const string input) {
  string s = input;
  while (s[0] == ' ') {
    s = s.substr(1);
  }
  if (s[0] == '\"' and s[s.size()-1] =='\"') {
    return true;
  }
  return false;
};

// Determines whether or not an input string can be
// treated as a a C character ('a'). Useful for determining
// how to translate a C macro which has no type.
bool CToFTypeFormatter::isChar(const string input) {
  string s = input;
  while (s[0] == ' ') {
    s = s.substr(1);
  }
  if (s[0] == '\'' and s[s.size()-1] =='\'') {
    return true;
  }
  return false;
};

// Returns true if the string input provided contains the
// word short, int, long, or char. Otherwise it returns false.
// Useful for determining how to translate a C macro which 
// does not have a type.
bool CToFTypeFormatter::isType(const string input) {
  // only support int short long char for now
  if (input == "short" or input == "long" or input == "char" or input == "int" or
      input.find(" int") != std::string::npos or 
      input.find(" short") != std::string::npos or
      input.find(" long") != std::string::npos or 
      input.find(" char") != std::string::npos) {
    return true;
  }
  return false;
};

// Returns a string buffer containing the Fortran equivalent of a C macro resembling a typedef.
// The Arguments and PresumedLoc are used to give information about the location of any
// errors that might occur during an attempted translation. The macro is translated into 
// a Fortran TYPE definition. This only supports int, shorts, chars, and longs. The TYPE
// will include one field which is [type_name]_C_CHAR/C_INT/C_LONG as appropriate given
// the type being declared.
string CToFTypeFormatter::createFortranType(const string macroName, const string macroVal, PresumedLoc loc, Arguments &args) {
  string ft_buffer;
  string type_id = "typeID_" + macroName ;
  
  // Create a new name for the transalated type.
  // The name may not include spaces. Erase them if they are there.
  size_t found = type_id.find_first_of(" ");
  while (found != string::npos) {
    type_id.erase(0, found + 1);  // Erase up to the space
    found=type_id.find_first_of(" ");
  }

  if (macroName[0] == '_') {
    if (args.getSilent() == false) {
      errs() << "Warning: Fortran names may not start with '_' ";
      errs() << macroName << " renamed h2m" << macroName << "\n";
      LineError(loc);
    }

    // Check the length of these lines to make sure they are legal under Fortran.
    ft_buffer += CheckLength("TYPE, BIND(C) :: h2m" + macroName+ "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
    if (macroVal.find("char") != std::string::npos) {
      ft_buffer += CheckLength("    CHARACTER(C_CHAR) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else if (macroVal.find("long") != std::string::npos) {
      ft_buffer += CheckLength("    INTEGER(C_LONG) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else if (macroVal.find("short") != std::string::npos) {
      ft_buffer += CheckLength("    INTEGER(C_SHORT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else {
      ft_buffer += CheckLength("    INTEGER(C_INT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    }
    ft_buffer += "END TYPE h2m" + macroName+ "\n";
  } else {
    // The CheckLength function is employed here to make sure lines are acceptable lengths
    ft_buffer = CheckLength("TYPE, BIND(C) :: " + macroName+ "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
    if (macroVal.find("char") != std::string::npos) {
      ft_buffer += CheckLength("    CHARACTER(C_CHAR) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else if (macroVal.find("long") != std::string::npos) {
      ft_buffer += CheckLength("    INTEGER(C_LONG) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else if (macroVal.find("short") != std::string::npos) {
      ft_buffer += CheckLength("    INTEGER(C_SHORT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    } else {
      ft_buffer += CheckLength("    INTEGER(C_INT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
          args.getSilent(), loc);
    }
    ft_buffer += "END TYPE " + macroName+ "\n";
  }
  return ft_buffer;
};

// -----------initializer VarDeclFormatter--------------------
VarDeclFormatter::VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  varDecl = v;
  // Because sloc is checked for validity prior to use, this should handle invalid locations
  if (varDecl->getSourceRange().getBegin().isValid()) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(varDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(varDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header.
  }
};

// In the event that a variable declaration has an initial value, this function
// attempts to find that initialization value and return it as a string. It handles
// poitners, reals, complexes, characters, ints. Arrays are defined here but actually
// handled elsewhere (I think). Typedefs and structs are not handled here.
string VarDeclFormatter::getInitValueASString() {
  string valString;

  if (varDecl->hasInit() and !isInSystemHeader) {
    if (varDecl->getType().getTypePtr()->isStructureType()) {
        // structure type skip
    } else if (varDecl->getType().getTypePtr()->isCharType()) {
        // single CHAR
      char character = varDecl->evaluateValue ()->getInt().getExtValue ();
      string cString;
      cString += character;
      valString = "\'" + cString + "\'";
    } else if (varDecl->getType().getTypePtr()->isIntegerType()) {
        // INT
      int intValue = varDecl->evaluateValue ()->getInt().getExtValue();
      valString = to_string(intValue);
    } else if (varDecl->getType().getTypePtr()->isRealType()) {
        // REAL
      valString = varDecl->evaluateValue ()->getAsString(varDecl->getASTContext(), varDecl->getType());
    } else if (varDecl->getType().getTypePtr()->isComplexType()) {
        // COMPLEX
      APValue *apVal = varDecl->evaluateValue ();
      if (apVal->isComplexFloat ()) {
        float real = apVal->getComplexFloatReal ().convertToFloat ();
        float imag = apVal->getComplexFloatImag ().convertToFloat ();
        valString = "(" + to_string(real) + "," + to_string(imag) +")";
      } else if (apVal->isComplexInt ()) {
        int real = apVal->getComplexIntReal ().getExtValue ();
        int imag = apVal->getComplexIntImag ().getExtValue ();
        valString = "(" + to_string(real) + "," + to_string(imag) +")";
      } 
    } else if (varDecl->getType().getTypePtr()->isPointerType()) {
      // POINTER 
      QualType pointerType = varDecl->getType();
      QualType pointeeType = pointerType.getTypePtr()->getPointeeType();
      if (pointeeType.getTypePtr()->isCharType()) {
        // string literal
        Expr *exp = varDecl->getInit();
        if (isa<ImplicitCastExpr> (exp)) {
          ImplicitCastExpr *ice = cast<ImplicitCastExpr> (exp);
          Expr *subExpr = ice->getSubExpr();
          if (isa<clang::StringLiteral> (subExpr)) {
            clang::StringLiteral *sl = cast<clang::StringLiteral> (subExpr);
            string str = sl->getString();
            valString = "\"" + str + "\"";
          }
        }

      } else {
        valString = "!" + varDecl->evaluateValue()->getAsString(varDecl->getASTContext(), varDecl->getType());
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Variable declaration initialization commented out:\n";
          errs() << valString << "\n";
          LineError(sloc); 
        }
      }
    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
      // ARRAY --- won't be used here bc it's handled by getFortranArrayDeclASString()
      Expr *exp = varDecl->getInit();
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc (),
          varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
      // comment out arrayText
      std::istringstream in(arrayText);
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: array contents " << line << " commented out \n";
          LineError(sloc);
        }
        valString += "! " + line + "\n";
      }
    } else {
      valString = "!" + varDecl->evaluateValue()->getAsString(varDecl->getASTContext(), varDecl->getType());
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Variable declaration initialization commented out:\n" << valString << " \n";
        LineError(sloc);
      }
    }
  }
  return valString;

};

// This function fetches the type and name of an array element through the 
// ast context. It then evaluates the elements and returns their values.
// Special care must be taken to convert a char array into chars rather than
// integers (the is_char variable sees to this). Note that the strings are
// passed in by reference so that their values can be assembled in this helper.
// The "shapes" are the array dimensions (ie (2, 4, 2)). 
void VarDeclFormatter::getFortranArrayEleASString(InitListExpr *ile, string &arrayValues,
    string arrayShapes, bool &evaluatable, bool firstEle, bool is_char) {
  ArrayRef<Expr *> innerElements = ile->inits ();
  // Assembles the shape of the array to be used
  if (firstEle) {
    size_t numOfInnerEle = innerElements.size();
    // Reverse the index if necessary by putting this size parameter at the front
    if (args.getArrayTranspose() == true) {
      arrayShapes = to_string(numOfInnerEle) + ", " + arrayShapes;
      arrayShapes_fin = arrayShapes;
    } else {
      arrayShapes += ", " + to_string(numOfInnerEle);
      arrayShapes_fin = arrayShapes;
    }
  }
  // Finds AST context for each array element
  for (auto it = innerElements.begin (); it != innerElements.end(); it++) {
    Expr *innerelement = (*it);
    if (innerelement->isEvaluatable (varDecl->getASTContext())) {
      evaluatable = true;
      clang::Expr::EvalResult r;
      innerelement->EvaluateAsRValue(r, varDecl->getASTContext());
      string eleVal = r.Val.getAsString(varDecl->getASTContext(), innerelement->getType());
      // We must convert this integer string into a char. This is annoying.
      if (is_char == true) {
        // This will throw an exception if it fails.
        int temp_val = std::stoi(eleVal);
        char temp_char = static_cast<char>(temp_val);
        // Put the character into place and surround it by quotes
        eleVal = "'";
        eleVal += temp_char;
        eleVal += "'";
      }

      if (arrayValues.empty()) {
        arrayValues = eleVal;
      } else {
        arrayValues += ", " + eleVal;
      }
    // Recursively calls to find the smallest element of the array
    } else if (isa<InitListExpr> (innerelement)) {
      InitListExpr *innerile = cast<InitListExpr> (innerelement);
      getFortranArrayEleASString(innerile, arrayValues, arrayShapes, evaluatable,
          (it == innerElements.begin()) and firstEle, is_char);
    } 
  }

  // We have inadvertently added an extra " ," to the front of this
  // string during our reversed iteration through the length. This
  // little erasure takes care of that.
  if (args.getArrayTranspose() == true) {
    arrayShapes.erase(arrayShapes.begin(), arrayShapes.begin() + 2);
  }
};

// Much more complicated function used to generate an array declaraion. 
// Syntax for C and Fortran arrays are completely different. The array 
// must be declared and initialization carried out if necessary.
string VarDeclFormatter::getFortranArrayDeclASString() {
  string arrayDecl = "";
  // This keeps system header pieces from leaking into the translation
  if (varDecl->getType().getTypePtr()->isArrayType() and !isInSystemHeader) {
    CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
  
    string temp_id = tf.getFortranIdASString(varDecl->getNameAsString());

    // Check for an illegal name length. Warn if it exists.
    string truncated_id;
    truncated_id = temp_id.substr(0, temp_id.find_first_of("("));
    CheckLength(truncated_id, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check to see whether we have seen this identifier before. If need be, comment
    // out the duplicate declaration.
    // We need to strip off the (###) for the test or we will end up testing
    // for a repeat of the variable n(4) rather than n if the decl is int n[4];

    if (RecordDeclFormatter::StructAndTypedefGuard(truncated_id) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << truncated_id;
        errs() << ", array declaration.\n";
        LineError(sloc);
      }
      arrayDecl = "! Skipping duplicate declaration of " + truncated_id + "\n!";
    }

    if (!varDecl->hasInit()) {
      // only declared, no initialization of the array takes place
      arrayDecl += tf.getFortranTypeASString(true) + ", public, BIND(C) :: ";
      arrayDecl += tf.getFortranIdASString(varDecl->getNameAsString()) + "\n";
    } else {
      // The array is declared and initialized.
      const ArrayType *at = varDecl->getType().getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType ();
      Expr *exp = varDecl->getInit();
      // Whether this is a char array or not will have to be checked several times.
      // This checks whether or not the "innermost" type is a char type.
      bool isChar = varDecl->getASTContext().getBaseElementType(e_qualType).getTypePtr()->isCharType();
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc(),
            varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
     // A char array might not be a string literal. A leading { character indicates this case.
     if (isChar == true && arrayText.front() != '{') {
        // handle stringliteral case
        // A parameter may not have a bind(c) attribute
        arrayDecl += tf.getFortranTypeASString(true) + ", parameter, public :: " +
            tf.getFortranIdASString(varDecl->getNameAsString()) + " = " + arrayText + "\n";
      } else {
        bool evaluatable = false;
        Expr *exp = varDecl->getInit();
        if (isa<InitListExpr> (exp)) {
          // initialize shape and values
          // format: INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/))
          string arrayValues;
          string arrayShapes;

          InitListExpr *ile = cast<InitListExpr>(exp);
          ArrayRef<Expr *> elements = ile->inits();
          size_t numOfEle = elements.size();
          arrayShapes = to_string(numOfEle);
          arrayShapes_fin = arrayShapes;
          for (auto it = elements.begin(); it != elements.end(); it++) {
            Expr *element = (*it);
            if (isa<InitListExpr> (element)) {
              // multidimensional array
              InitListExpr *innerIle = cast<InitListExpr>(element);
              // This function will recursively find the dimensions. The arrayValues and
              // arrayShapes are initialized in the function.
              getFortranArrayEleASString(innerIle, arrayValues, arrayShapes, evaluatable,
                  it == elements.begin(), isChar);
            } else {
              if (element->isEvaluatable(varDecl->getASTContext())) {
                // one dimensional array
                clang::Expr::EvalResult r;
                element->EvaluateAsRValue(r, varDecl->getASTContext());
                string eleVal = r.Val.getAsString(varDecl->getASTContext(), e_qualType);

                // In the case of a char array initalized ie {'a', 'b',...} we require a 
                // check and a conversion of the int value produced into a char value which
                // is valid in a Fortran character array.
                if (isChar == true) {
                  // On a failed conversion, this throws an exception. That shouldn't happen.
                  int temp_val = std::stoi(eleVal);
                  char temp_char = static_cast<char>(temp_val);
                  eleVal = "'";
                  eleVal += temp_char;
                  eleVal += "'";
                }

                if (it == elements.begin()) {
                  // first element
                  evaluatable = true;
                  arrayValues = eleVal;
                } else {
                  arrayValues += ", " + eleVal;
                }

              }
            }
          } //<--end iteration (one pass through the array elements)
          if (!evaluatable) {
            // We can't translate this array because we can't evaluate its values to
           // get fortran equivalents. We comment out the declaration.
            string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(varDecl->getSourceRange()),
                rewriter.getSourceMgr(), LangOptions(), 0);
                // comment out arrayText
            std::istringstream in(arrayText);
            for (std::string line; std::getline(in, line);) {
              arrayDecl += "! " + line + "\n";
              if (args.getQuiet() == false && args.getSilent() == false) {
                errs() << "Warning: array text " << line << " commented out.\n";
                LineError(sloc);
              }
            }
          } else {
                //INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/))
            arrayDecl += tf.getFortranTypeASString(true)+", BIND(C) :: "+ varDecl->getNameAsString() +"("+arrayShapes_fin+")"+" = RESHAPE((/"+arrayValues+"/), (/"+arrayShapes_fin+"/))\n";

          }
        }
      }     
    }
  }
  // Check for lines which exceed the Fortran maximum. The helper will warn
  // if they are found and Silent is false. The +1 is because of the newline character
  // which doesn't count towards line length.
  CheckLength(arrayDecl, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);

  return arrayDecl;
};



// Hanldes various variable declarations and returns them as fortran strings. Helper
// functions are called for arrays which are difficult to format. The type is 
// fetched with the helper function getFortranTypeASString. Any illegal identifiers (ie _thing)
// are prepended with "h2m" to become legal fortran identifiers.
string VarDeclFormatter::getFortranVarDeclASString() {
  string vd_buffer = "";
  string identifier = "";   // Will eventually hold the variable's name
  
  if (!isInSystemHeader) {  // This appears to protect local headers from having system
   // headers leak into the definitions
    // This is a declaration of a TYPE(stuctured_type) variable
    if (varDecl->getType().getTypePtr()->isStructureType()) {
      // structure type
      RecordDecl *rd = varDecl->getType().getTypePtr()->getAsStructureType()->getDecl();
      // RecordDeclFormatter rdf(rd, rewriter, args);
      // rdf.setTagName(varDecl->getNameAsString());  // Done: Experiment with this line
      // vd_buffer = rdf.getFortranStructASString();  // Done: This line is wrong. Fix.
      // Create a structure defined in the module file.
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C) :: " + identifier + "\n";

      // The following are checks for potentially illegal characters which might be
      // at the begining of the anonymous types. These types must be commented out.
      // It also looks for the "anonymous at" qualifier which might be present if 
      // the other tell-tale signs are not.
      string f_type = tf.getFortranTypeASString(false);
      if (f_type.front() == '/' || f_type.front() == '_' || f_type.front() == '\\' ||
          f_type.find("anonymous at") != string::npos) {
        vd_buffer = "! " + vd_buffer;
      }
      // We have seen something with this name before. This will be a problem.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Variable declaration with name conflict, " << identifier << ", commented out.";
          LineError(sloc);
        }
        vd_buffer = "!" + vd_buffer;
        vd_buffer = "! Commenting out name conflict.\n" + vd_buffer;
        return vd_buffer;  // Skip the name and length checks. It's commented out anyway.
      }

    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
        // handle initialized numeric array specifically
        // length, name, and identifier repetition checks are carried out in the helper
        vd_buffer = getFortranArrayDeclASString();
    } else if (varDecl->getType().getTypePtr()->isPointerType() and 
      varDecl->getType().getTypePtr()->getPointeeType()->isCharType()) {
      // string declaration
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType().getTypePtr()->getPointeeType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      if (identifier.front() == '_') {
         if (args.getSilent() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         identifier = "h2m" + identifier;
      }
      // Detrmine the state of the declaration. Is there something declared? Is it commented out?
      // Create the declaration in correspondence with this.
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C) :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C) :: " + identifier + " " + value + "\n";
      } else {
        // A parameter may not have a bind(c) attribute.
        vd_buffer = tf.getFortranTypeASString(true) + ", parameter, public :: " + identifier + " = " + value + "\n";
      }
      // We have seen something with this name before. This will be a problem.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Variable declaration with name conflict, " << identifier << ", commented out.";
          LineError(sloc);
        }
        vd_buffer = "!" + vd_buffer;
        vd_buffer = "! Commenting out name conflict.\n" + vd_buffer;
        return vd_buffer;  // Skip the name and length checks. It's commented out anyway.
      }
 
      // If it is not a structure, pointer, or array, proceed with no special treatment.
    } else {
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      if (identifier.front() == '_') {
         if (args.getSilent() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         identifier = "h2m" + identifier;
      } 
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C) :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C) :: " + identifier + " " + value + "\n";
      } else {
        // A parameter may not have a bind(c) attribute
        vd_buffer = tf.getFortranTypeASString(true) + ", parameter, public :: " + identifier + " = " + value + "\n";
      }
      // We have seen something with this name before. This will be a problem.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Variable declaration with name conflict, " << identifier << ", commented out.";
          LineError(sloc);
        }
        vd_buffer = "!" + vd_buffer;
        vd_buffer = "! Commenting out name conflict.\n" + vd_buffer;
        return vd_buffer;  // Skip the name and length checks. It's commented out anyway.
      }
    }
    
    // Identifier may be initialized to "". This will cause no harm.
    CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // As we check for valid fortran name and line lengths, we add one to account for the
    // presence of the newline character.
    CheckLength(vd_buffer, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
  }
  return vd_buffer;
};

// -----------initializer Typedef--------------------
TypedefDeclFormater::TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  typedefDecl = t;
  isLocValid = typedefDecl->getSourceRange().getBegin().isValid();
  // sloc, if uninitialized, will be an invalid location. Because it is always passed to a function
  // which checks validity, this should be a fine way to guard against an invalid location.
  if (isLocValid) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(typedefDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(typedefDecl->getSourceRange().getBegin());
  }  
  
};

// From a C typedef, a string representing a Fortran pseudo-typedef is created. The fortran equivalent
// is a type with only one field. The name of this field is name_type (ie name_C_INT), depending
// on the type. Note that this function will be called for Structs and Enums as well, but that they
// will be skipped and handled elsewhere (in recordDeclFormatter). A typedef with an illegal 
// name will be prepended with "h2m" to become legal fortran.
// This function will check for name duplication. In the case that this is a duplicate identifier,
// a string containing a comment will be returned (no definition will be provided).
string TypedefDeclFormater::getFortranTypedefDeclASString() {
  string typdedef_buffer;
  if (isLocValid and !isInSystemHeader) {  // Keeps system files from leaking in
  // if (typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isStructureType()
  //  or typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isEnumeralType ()) {
  // } else {
  // The above commented out section appeared with a comment indicating that struct/enum
  // typedefs would be handled in the RecordDeclFormatter section. This does not appear
  // to be the case.
    TypeSourceInfo * typeSourceInfo = typedefDecl->getTypeSourceInfo();
    CToFTypeFormatter tf(typeSourceInfo->getType(), typedefDecl->getASTContext(), sloc, args);
    string identifier = typedefDecl->getNameAsString();
    if (identifier.front() == '_') {  // This identifier has an illegal _ at the begining.
      string old_identifier = identifier;
      identifier = "h2m" + identifier;  // Prepend h2m to fix the problem.
      if (args.getSilent() == false) {  // Warn about the renaming unless silenced.
        errs() << "Warning: illegal identifier " << old_identifier << " renamed " << identifier << "\n";
        LineError(sloc);
      }
    }
    

    // Check to make sure the identifier is not too lone
    CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    typdedef_buffer = "TYPE, BIND(C) :: " + identifier + "\n";
    // Because names in typedefs may collide with the typedef name, 
    // suffixes are appended to the internal member of the typedef.
    if (args.getSilent() == false) {
      errs() << "Warning: due to name collisions during typdef translation, " << identifier;
      errs() <<  "\nrenamed " << identifier << tf.getFortranTypeASString(false) << "\n";
      LineError(sloc);
    }
    string to_add = "    "+ tf.getFortranTypeASString(true) + "::" + identifier+"_"+tf.getFortranTypeASString(false) + "\n";
    CheckLength(identifier + "_" + tf.getFortranTypeASString(false), CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check for an illegal length. The \n character is the reason for the +1. It doesn't count
    // towards line length.
    CheckLength(to_add, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
    typdedef_buffer += to_add;
    typdedef_buffer += "END TYPE " + identifier + "\n";
  //  }
    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary.
    if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
        LineError(sloc);
      }
      string temp_buf = typdedef_buffer;
      typdedef_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
      std::istringstream in(temp_buf);
      // Loops through the buffer like a line-buffered stream and comments it out
      for (std::string line; std::getline(in, line);) {
        typdedef_buffer += "! " + line + "\n";
      }
      
    }
  } 
  return typdedef_buffer;
};

// -----------initializer EnumDeclFormatter--------------------
EnumDeclFormatter::EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  enumDecl = e;
  // Becasue sloc is only ever passed to a function which checks its validity, this should be a fine
  // way to deal with an invalid location. An empty sloc is an invalid location.
  if (enumDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(enumDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(enumDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
};

// From a C enumerated type, a fotran ENUM is created. The names are prepended
// with h2m if they begin with an underscore. The function loops through all 
// the members in the enumerator and adds them into place. Note that this can
// resut in serious problems if the enumeration is too large.
string EnumDeclFormatter::getFortranEnumASString() {
  string enum_buffer;
  if (!isInSystemHeader) {  // Keeps definitions in system headers from leaking into the translation
    string enumName = enumDecl->getNameAsString();
    // Check the length of the name to make sure it is valid Fortran
    // Note that it would be impossible for this line to be an illegal length unless
    // the variable name were hopelessly over the length limit.
    CheckLength(enumName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    enum_buffer = "ENUM, BIND( C )\n";
    // enum_buffer += "    enumerator :: ";  // Removed when changes were made to allow unlimited enum length
    // Cycle through the pieces of the enum and translate them into fortran
    for (auto it = enumDecl->enumerator_begin (); it != enumDecl->enumerator_end (); it++) {
      string constName = (*it)->getNameAsString ();
      if (constName.front() == '_') {  // The name begins with an illegal underscore.
        string old_constName = constName;
        constName = "h2m" + constName;
        if (args.getSilent() == false) {
          errs() << "Warning: illegal enumeration identfier " << old_constName << " renamed ";
          errs() << constName << "\n";
          LineError(sloc);
        }
      }
      int constVal = (*it)->getInitVal ().getExtValue ();  // Get the initialization value
      // Check for a valid name length
      CheckLength(constName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
      // Problem! We have seen an identifier with this name before! Comment out the line
      // and warn about it.
      if (RecordDeclFormatter::StructAndTypedefGuard(constName) == false) { 
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << constName << ", enum member.\n";
          LineError(sloc);
        }
        enum_buffer += "! Skipping duplicate identifier.";
        enum_buffer +=  "    ! enumerator :: " + constName + "=" + to_string(constVal) + "\n";
      } else {  // Otherwise, just add it on the buffer
        enum_buffer += "    enumerator :: " + constName + "=" + to_string(constVal) + "\n";
      }
    }
    // erase the redundant colon  // This erasing and adding back in of a newline is obsolete with the new format
    // enum_buffer.erase(enum_buffer.size()-2);
    // enum_buffer += "\n";
    // Put the actual name of the enumerator as an uninitialized member of the enumerated type
    if (!enumName.empty()) {
      if (enumName.front() == '_') {  // Illegal underscore beginning the name!
        string old_enumName = enumName;
        enumName = "h2m" + enumName;  // Prepend h2m to fix the problem
        if (args.getSilent() == false) {  // Warn unless silenced
          errs() << "Warning: illegal enumeration identifier " << old_enumName << " renamed " << enumName << "\n";
          LineError(sloc); 
        } 
      }
      enum_buffer += "    enumerator " + enumName+"\n";
    } else {  // I don't think this is actually a problem for potential illegal underscores.
      string identifier = enumDecl-> getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // This checks to make sure that this is not an anonymous enumeration
      if (identifier.find("anonymous at") == string::npos) {
        enum_buffer += "    enumerator " + identifier+"\n";
      }
    }
    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary.
    if (RecordDeclFormatter::StructAndTypedefGuard(enumName) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << enumName << "\n";
        LineError(sloc);
      }
      // Comment out the declaration by stepping through and appending ! before newlines
      // to avoid duplicate identifier collisions.
      string temp_buf = enum_buffer;
      enum_buffer = "\n! Duplicate declaration of " + enumName + ", ENUM, skipped.\n";
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        enum_buffer += "! " + line + "\n";
      }
      // Add in a few last details... and return to avoid having END ENUM pasted on the end
      enum_buffer += "! END ENUM\n";
      return(enum_buffer);
    }

    enum_buffer += "END ENUM\n";
  }

  return enum_buffer;
};

// -----------initializer RecordDeclFormatter--------------------

RecordDeclFormatter::RecordDeclFormatter(RecordDecl* rd, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  recordDecl = rd;
  // Because sloc is checked for validity prior to use, this should be a fine way to deal with
  // an invalid source location
  if (recordDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(recordDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(recordDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it's not anywhere, it isn't in a system header
  }
};

bool RecordDeclFormatter::isStruct() {
  return structOrUnion == STRUCT;
};

bool RecordDeclFormatter::isUnion() {
  return structOrUnion == UNION;
};

// tag_name is the name used as 'struct name' which C
// stores in a seperate symbol table. Structs are usually
// referred to by typedef provided, simpler names
void RecordDeclFormatter::setTagName(string name) {
  tag_name = name;
}

// A struct or union obtains its fields from this function
// which creates and uses a type formatter for each field in turn and adds
// each onto the buffer as it iterates through all the fields.
string RecordDeclFormatter::getFortranFields() {
  string fieldsInFortran = "";
  if (!recordDecl->field_empty()) {
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); it++) {
      CToFTypeFormatter tf((*it)->getType(), recordDecl->getASTContext(), sloc, args);
      string identifier = tf.getFortranIdASString((*it)->getNameAsString());
      if (identifier.front() == '_') {
        if (args.getSilent() == false) {
          errs() << "Warning: invalid struct field name " << identifier;
          errs() << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;
      }
      // Make sure that the field's identifier isn't too long for a fortran name
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);

      fieldsInFortran += "    " + tf.getFortranTypeASString(true) + " :: " + identifier + "\n";
    }
  }
  return fieldsInFortran;
}

// The procedure for any of the four sorts of structs/typedefs is fairly
// similar, but anonymous structs need to be handled specially. This 
// function puts together the name of the struct as well as the fields fetched
// from the getFortranFields() function above. All illegal names are prepended
// with h2m. Checks are made for duplicate names.
string RecordDeclFormatter::getFortranStructASString() {
  // initalize mode here
  setMode();

  string rd_buffer;
  if (!isInSystemHeader) {  // Prevents system headers from leaking in
    string fieldsInFortran = getFortranFields();
    if (fieldsInFortran.empty()) {
      rd_buffer = "! struct without fields may cause warnings\n";
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Warning: struct without fields may cause warnings: \n";
        LineError(sloc);
      }
    }

    if (mode == ID_ONLY) {
      string identifier = recordDecl->getNameAsString();
            if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration by stepping through and appending ! before newlines
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == TAG_ONLY) {
      string identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration by stepping through and appending ! before newlines
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ID_TAG) {
      string identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration so that it is present in the output file
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";    
    } else if (mode == TYPEDEF) {
      string identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid typedef name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration so that it is present in the output file
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ANONYMOUS) {
      // Note that no length checking goes on here because there's no need. 
      // This will all be commented out anyway.
      string identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // Erase past all the spaces so that only the name remains (ie get rid of the "struct" part)
      size_t found = identifier.find_first_of(" ");
      while (found!=string::npos) {
        identifier.erase(0, found + 1);
        found=identifier.find_first_of(" ");
      }
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // We have previously seen a declaration with this name. This shouldn't be possible, so don't 
      // worry about commenting it out. It's commented out anyway.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
      }

      rd_buffer += "! ANONYMOUS struct may or may not have a declared name\n";
      string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
      // Comment out the concents of the anonymous struct. There is no good way to guess at a name for it.
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        rd_buffer += "! " + line + "\n";
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line in anonymous struct" << line << " commented out \n";
          LineError(sloc);
        }
      }
    }
  }
  return rd_buffer;    
};

// Determines what sort of struct we are dealing with. The differences
// are subtle. If you need to understand what these modes are, you will
// have to play around with some structs. 
void RecordDeclFormatter::setMode() {
  // int ANONYMOUS = 0;
  // int ID_ONLY = 1;
  // int TAG_ONLY = 2;
  // int ID_TAG = 3;

  if (!(recordDecl->getNameAsString()).empty() and !tag_name.empty()) {
    mode = ID_TAG;
  } else if (!(recordDecl->getNameAsString()).empty() and tag_name.empty()) {
    mode = ID_ONLY;
  } else if ((recordDecl->getNameAsString()).empty() and !tag_name.empty()) {
    mode = TAG_ONLY;
  } else if ((recordDecl->getNameAsString()).empty() and tag_name.empty()) {
    string identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
    if (identifier.find(" ") != string::npos) {
      mode = ANONYMOUS;
    } else {
      // is a identifier
      mode = TYPEDEF;
    }
    
  }
};




// -----------initializer FunctionDeclFormatter--------------------
FunctionDeclFormatter::FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  funcDecl = f;
  returnQType = funcDecl->getReturnType();
  params = funcDecl->parameters();
  // Because sloc is checked for validity prior to use, this should be a fine way to deal with
  // invalid locations
  if (funcDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(funcDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(funcDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
};

// for inserting types to "USE iso_c_binding, only: <<< c_ptr, c_int>>>""
// This function determines the types which are passed into a function so that
// the above demonstrated syntax can be used to establish proper fortran binding.
// Each type present will only be mentioned once.
string FunctionDeclFormatter::getParamsTypesASString() {
  string paramsType;
  QualType prev_qt;
  std::vector<QualType> qts;
  bool first = true;
  // loop through all arguments
  for (auto it = params.begin(); it != params.end(); it++) {
    if (first) {
      prev_qt = (*it)->getOriginalType();
      qts.push_back(prev_qt);
      CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(), sloc, args);
      if (tf.getFortranTypeASString(false).find("C_") != std::string::npos) {
        paramsType = tf.getFortranTypeASString(false);
      }
      first = false;

      // add the return type too
      CToFTypeFormatter rtf(returnQType, funcDecl->getASTContext(), sloc, args);
      if (!returnQType.getTypePtr()->isVoidType()) {
        if (rtf.isSameType(prev_qt)) {
        } else {
          // check if type is in the vector
          bool add = true;
          for (auto v = qts.begin(); v != qts.end(); v++) {
            if (rtf.isSameType(*v)) {
              add = false;
            }
          }
          if (add) {
            if (rtf.getFortranTypeASString(false).find("C_") != std::string::npos) {
              if (paramsType.empty()) {
                paramsType += rtf.getFortranTypeASString(false);
              } else {
                paramsType += (", " + rtf.getFortranTypeASString(false));
              }
              
            }
            
          }
        }
        prev_qt = returnQType;
        qts.push_back(prev_qt);
      }

    } else {
      CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(), sloc, args);
      if (tf.isSameType(prev_qt)) {
      } else {
          // check if type is in the vector
        bool add = true;
        for (auto v = qts.begin(); v != qts.end(); v++) {
          if (tf.isSameType(*v)) {
            add = false;
          }
        }
        if (add) {
          if (tf.getFortranTypeASString(false).find("C_") != std::string::npos) {
            paramsType += (", " + tf.getFortranTypeASString(false));
          }
        }
      }
      prev_qt = (*it)->getOriginalType();
      qts.push_back(prev_qt);
    }        

  }
  return paramsType;
};

// for inserting variable decls "<<<type(c_ptr), value :: arg_1>>>"
// This function gives the parameters passed to the function in 
// the form needed after the initial function declaration to 
// specify their types, intents, etc.
// A CToFTypeFormatter is created for each as the function loops
// through all the parameters.
string FunctionDeclFormatter::getParamsDeclASString() { 
  string paramsDecl;
  int index = 1;
  for (auto it = params.begin(); it != params.end(); it++) {
    // if the param name is empty, rename it to arg_index
    string pname = (*it)->getNameAsString();
    if (pname.empty()) {
      pname = "arg_" + to_string(index);
    }
    if (pname.front() == '_') {  // Illegal character. Append a prefix.
      string old_pname = pname;
      pname = "h2m" + pname;
      // This would be a duplicate warning
      // if (args.getSilent() == false && args.getQuiet() == false) {
      //  errs() << "Warning: Illegal parameter identifier " << old_pname << " renamed ";
      //  errs() << pname << "\n";
      //  LineError(sloc);
      // }
    }
    // Check for a valid name length for the dummy variable.
    CheckLength(pname, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    
    CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(), sloc, args);
    // in some cases parameter doesn't have a name
    paramsDecl += "    " + tf.getFortranTypeASString(true) + ", value" + " :: " + pname + "\n";
    // need to handle the attribute later - I don't know what this commment means
    // Similarly, check the length of the declaration line to make sure it is valid Fortran.
    // Note that the + 1 in length is to account for the newline character.
    CheckLength(pname, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
    index++;
  }
  return paramsDecl;
}

// for inserting variable decls "getline(<<<arg_1, arg_2, arg_3>>>)"
// This function loops through variables in a function declaration and
// returns them in a form suitable to be used in the initial line of that
// declaration. It only needs to list their names interspersed with 
// commas and knows nothing about their types.
string FunctionDeclFormatter::getParamsNamesASString() { 
  string paramsNames;
  int index = 1;
  for (auto it = params.begin(); it != params.end(); it++) {
    if (it == params.begin()) {
      // if the param name is empty, rename it to arg_index
      //uint64_t  getTypeSize (QualType T) const for array!!!
      string pname = (*it)->getNameAsString();
      if (pname.empty()) {
        pname = "arg_" + to_string(index);
      }
      if (pname.front() == '_') {  // Illegal character. Append a prefix.
        string old_pname = pname;
        pname = "h2m" + pname;
        if (args.getSilent() == false) {
          errs() << "Warning: Illegal parameter identifier " << old_pname << " renamed ";
          errs() << pname << "\n";
          LineError(sloc);
        }
      }
      paramsNames += pname;
    } else { // parameters in between
      // if the param name is empty, rename it to arg_index
      string pname = (*it)->getNameAsString();
      if (pname.empty()) {
        pname = "arg_" + to_string(index);
      }
      if (pname.front() == '_') {  // Illegal character. Append a prefix.
        string old_pname = pname;
        pname = "h2m" + pname;
        if (args.getSilent() == false) {
          errs() << "Warning: Illegal parameter name " << old_pname << " renamed ";
          errs() << pname << "\n";
          LineError(sloc);
        }
      }
      paramsNames += ", " + pname; 
    }
    index ++;
  }
  return paramsNames;
};

bool FunctionDeclFormatter::argLocValid() {
  for (auto it = params.begin(); it != params.end(); it++) {
    if ((*it)->getSourceRange().getBegin().isValid()) {
      return true;
    } else {
      return false;
    }
  }
  return true;
};


// return the entire function decl in fortran
// Using helpers to fetch the names of the parameters and their 
// full declarations and attributes, this function translates
// an entire C function into either a Fotran function or subroutine
// depending on the return value (void return means subroutine). It
// must also decide what sorts of iso_c_binding to use and relies
// on an earlier defined function.
string FunctionDeclFormatter::getFortranFunctDeclASString() {
  string fortranFunctDecl;
  if (!isInSystemHeader and argLocValid()) {
    string funcType;
    string paramsString = getParamsTypesASString();
    string imports;
    if (!paramsString.empty()) {
      // CheckLength just returns the same string, but it will make sure the line is not too
      // long for Fortran and it will warn if needed and not silenced.
      imports = CheckLength("    USE iso_c_binding, only: " + getParamsTypesASString() + "\n",
          CToFTypeFormatter::line_max, args.getSilent(), sloc);
    } else {
      imports = "    USE iso_c_binding\n";
    }
    imports +="    import\n";
    
    // check if the return type is void or not
    // A void type means we create a subroutine. Otherwise a function is written.
    if (returnQType.getTypePtr()->isVoidType()) {
      funcType = "SUBROUTINE";
    } else {
      CToFTypeFormatter tf(returnQType, funcDecl->getASTContext(), sloc, args);
      funcType = tf.getFortranTypeASString(true) + " FUNCTION";
    }
    string funcname = funcDecl->getNameAsString();
    if (funcname.front() == '_') {  // We have an illegal character in the identifier
      string oldfuncname = funcname;
      funcname = "h2m" + funcname;  // Prepend h2m to fix the problem
      if (args.getSilent() == false) {
        errs() << "Warning: invalid function name " << oldfuncname << " renamed " << funcname << "\n";
        LineError(sloc);
      }
    }
    // Check to make sure the function's name isn't too long. Warn if necessary.
    CheckLength(funcname, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check to make sure this declaration line isn't too long. It well might be.
    fortranFunctDecl = CheckLength(funcType + " " + funcname + "(" + getParamsNamesASString() + 
        ")" + " bind (C)\n", CToFTypeFormatter::line_max, args.getSilent(), sloc);
    
    fortranFunctDecl += imports;
    fortranFunctDecl += getParamsDeclASString();
    // preserve the function body as comment
    if (funcDecl->hasBody()) {
      Stmt *stmt = funcDecl->getBody();
      clang::SourceManager &sm = rewriter.getSourceMgr();
      // comment out the entire function {!body...}
      string bodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(stmt->getSourceRange()),
          sm, LangOptions(), 0);
      string commentedBody;
      std::istringstream in(bodyText);
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line " << line << " commented out \n";
          LineError(sloc);
        }
        commentedBody += "! " + line + "\n";
      }
      fortranFunctDecl += commentedBody;

    }
    if (returnQType.getTypePtr()->isVoidType()) {
      fortranFunctDecl += "END SUBROUTINE " + funcname + "\n\n";   
    } else {
      fortranFunctDecl += "END FUNCTION " + funcname + "\n\n";
    }
   
    // The guard function checks for duplicate identifiers. This might 
    // happen because C is case sensitive. It shouldn't happen often, but if
    // it does, the duplicate declaration needs to be commented out.
    if (RecordDeclFormatter::StructAndTypedefGuard(funcname) == false) { 
      if (args.getSilent() == false) {
        errs() << "Warning: duplicate declaration of " << funcname << ", FUNCTION, skipped.\n";
        LineError(sloc);
      }
      string temp_buf = fortranFunctDecl;
      fortranFunctDecl = "\n! Duplicate declaration of " + funcname + ", FUNCTION, skipped.\n";
      std::istringstream in(temp_buf);
      // Loops through the buffer like a line-buffered stream and comments it out
      for (std::string line; std::getline(in, line);) {
        fortranFunctDecl += "! " + line + "\n";
      }
    }
    
  }

  return fortranFunctDecl;
};

// -----------initializer MacroFormatter--------------------
// The preprocessor is used to find the macro names in the source files. The macro is 
// more difficult to process. Both its name and definition are fetched using the lexer.
MacroFormatter::MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci,
    Arguments &arg) : md(md), args(arg), ci(ci) {
    const MacroInfo *mi = md->getMacroInfo();
    SourceManager& SM = ci.getSourceManager();

    // define macro properties
    isObjectOrFunction = mi->isObjectLike();
   
    // This should be a fine way to deal with invalid locations because sloc is checked for validity
    // before it is used.
    if (mi->getDefinitionLoc().isValid()) {
      sloc = SM.getPresumedLoc(mi->getDefinitionLoc());
      isInSystemHeader = SM.isInSystemHeader(mi->getDefinitionLoc());
    } else {
      isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
    }

    // source text
    macroName = Lexer::getSourceText(CharSourceRange::getTokenRange(MacroNameTok.getLocation(), MacroNameTok.getEndLoc()), SM, LangOptions(), 0);
    macroDef = Lexer::getSourceText(CharSourceRange::getTokenRange(mi->getDefinitionLoc(), mi->getDefinitionEndLoc()), SM, LangOptions(), 0);
    
    // strangely there might be a "(" follows the macroName for function macros, remove it if there is
    if (macroName.back() == '(') {
      //args.getOutput().os() << "unwanted parenthesis found, remove it \n";
      macroName.erase(macroName.size()-1);
    }

    // get value for object macro
    bool frontSpace = true;
    for (size_t i = macroName.size(); i < macroDef.size(); i++) {
      if (macroDef[i] != ' ') {
        frontSpace = false;
        macroVal += macroDef[i];
      } else if (frontSpace == false) {
        macroVal += macroDef[i];
      }
    }
}

bool MacroFormatter::isObjectLike() {
  return isObjectOrFunction;
};
bool MacroFormatter::isFunctionLike() {
  return !isObjectOrFunction;
};

// return the entire macro in fortran
// Macros which are object like, meaning similar to Chars, Strings, integers, doubles, etc
// can be translated. Macros which are empty are defined as positive bools. Function-like
// macros can be translated as functions or subroutines. However, it is not always possible
// to translate a macro, in which case the line must be commented out.
string MacroFormatter::getFortranMacroASString() {
  string fortranMacro;
  bool duplicateIdentifier = false;

  // If we are not in the main file, don't include this. Just
  // return an empty string.  If the Together argument is specified, include it anyway.
  if (ci.getSourceManager().isInMainFile(md->getMacroInfo()->getDefinitionLoc()) == false
      && args.getTogether() == false) {
    return "";
  } 
  if (!isInSystemHeader) {  // Keeps macros from system headers from bleeding into the file
    // remove all tabs
    macroVal.erase(std::remove(macroVal.begin(), macroVal.end(), '\t'), macroVal.end());

    // handle object first, this means definitions of parameters of int, char, double... types
    if (isObjectLike()) {
      // analyze type
      if (!macroVal.empty()) {
        if (CToFTypeFormatter::isString(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed " << "h2m" << macroName << "\n";
              LineError(sloc);
            }
            fortranMacro += "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: " + macroName + " = " + macroVal + "\n";
          }
        
        } else if (CToFTypeFormatter::isChar(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed " << "h2m" << macroName << "\n";
              LineError(sloc);
            }
            fortranMacro += "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }
        
        } else if (CToFTypeFormatter::isIntLike(macroVal)) {
          // invalid chars
          if (macroVal.find_first_of("UL") != std::string::npos) {
            if (args.getSilent() == false) {
              errs() << "Warning: Macro with value including UL detected. ";
              errs() << macroName << " Is invalid.\n";
              LineError(sloc);
            }
            fortranMacro = "!INTEGER(C_INT), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          } else if (macroName.front() == '_') {  // Invalid underscore as first character
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran name with invalid characters detected. ";
              errs() << macroName << " renamed h2m" << macroName << "\n"; 
              LineError(sloc);
            }
            fortranMacro = "INTEGER(C_INT), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else if (macroVal.find("x") != std::string::npos) {
            size_t x = macroVal.find_last_of("x");
            string val = macroVal.substr(x+1);
            val.erase(std::remove(val.begin(), val.end(), ')'), val.end());
            val.erase(std::remove(val.begin(), val.end(), '('), val.end());
            fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName + " = int(z\'" + val + "\')\n";
          } else {
            fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }

        } else if (CToFTypeFormatter::isDoubleLike(macroVal)) {
          if (macroVal.find_first_of("FUL") != std::string::npos) {
            if (args.getSilent() == false) {
              errs() << "Warning: macro with value including FUL detected. ";
              errs() << macroName << " Is invalid.\n";
              LineError(sloc);
            }
            fortranMacro = "!REAL(C_DOUBLE), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          } else if (macroName.front() == '_') {
             if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed h2m" << macroName << ".\n";
              LineError(sloc);
            }
            fortranMacro = "REAL(C_DOUBLE), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "REAL(C_DOUBLE), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }
          
        } else if (CToFTypeFormatter::isType(macroVal)) {
          // only support int short long char for now
          fortranMacro = CToFTypeFormatter::createFortranType(macroName, macroVal, sloc, args);

        } else {
          std::istringstream in(macroDef);
          for (std::string line; std::getline(in, line);) {
            if (args.getQuiet() == false && args.getSilent() == false) {
              errs() << "Warning: line " << line << " commented out\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        // Check the length of the lines of all object like macros prepared.
        CheckLength(fortranMacro, CToFTypeFormatter::line_max, args.getSilent(), sloc);
    } else { // macroVal.empty(), make the object a bool positive
      if (macroName[0] == '_') {
        if (args.getSilent() == false) {
          errs() << "Warning: Fortran names may not start with an underscore. ";
          errs() << macroName << " renamed h2m" << macroName << ".\n";
          LineError(sloc);
        }
        fortranMacro += "INTEGER(C_INT), parameter, public :: h2m" + macroName  + " = 1 \n";
      } else {
        fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName  + " = 1 \n";
      }
      // Check the length of the lines of all the empty macros prepared.
      CheckLength(fortranMacro, CToFTypeFormatter::line_max, args.getSilent(), sloc);
    }


    } else {
        // function macro
      size_t rParen = macroDef.find(')');
      string functionBody = macroDef.substr(rParen+1, macroDef.size()-1);
      if (macroName[0] == '_') {
        fortranMacro += "INTERFACE\n";
        if (args.getSilent() == false) {
          errs() << "Warning: fortran names may not start with an underscore ";
          errs() << macroName << " renamed h2m" << macroName << "\n";
          LineError(sloc);
        }
        if (md->getMacroInfo()->arg_empty()) {
          fortranMacro += "SUBROUTINE h2m" + macroName + "() bind (C)\n";
        } else {
          fortranMacro += "SUBROUTINE h2m"+ macroName + "(";
          for (auto it = md->getMacroInfo()->arg_begin (); it != md->getMacroInfo()->arg_end (); it++) {
            string argname = (*it)->getName();
            if (argname.front() == '_') {  // Illegal character in argument name
              if (args.getSilent() == false) {
                errs() << "Warning: fortran names may not start with an underscore. Macro argument ";
                errs() << argname << " renamed h2m" << argname << "\n";
                LineError(sloc);
              }
              argname = "h2m" + argname;  // Fix the problem by prepending h2m
            }
            fortranMacro += argname;
            fortranMacro += ", ";
          }
                    // erase the redundant colon
          fortranMacro.erase(fortranMacro.size()-2);
          fortranMacro += ") bind (C)\n";
          // Check that this line is not too long. Take into account the fact that the
          // characters INTERFACE\n alleady at the begining take up 10 characters and the 
          // newline just added uses another one.
          CheckLength(fortranMacro, CToFTypeFormatter::line_max + 11, args.getSilent(), sloc);

        }
        if (!functionBody.empty()) {  // Comment out the body of the function-like macro 
          std::istringstream in(functionBody);
          for (std::string line; std::getline(in, line);) {
            if (args.getSilent() == false && args.getQuiet() == false) {
              errs() << "Warning: line " << line << " commented out.\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        fortranMacro += "END SUBROUTINE h2m" + macroName + "\n";
        fortranMacro += "END INTERFACE\n";
      } else {
        fortranMacro = "INTERFACE\n";
        if (md->getMacroInfo()->arg_empty()) {
          fortranMacro += "SUBROUTINE "+ macroName + "() bind (C)\n";
        } else {
          fortranMacro += "SUBROUTINE "+ macroName + "(";
          for (auto it = md->getMacroInfo()->arg_begin (); it != md->getMacroInfo()->arg_end (); it++) {
            string argname = (*it)->getName();
            if (argname.front() == '_') {
              if (args.getSilent() == false) { 
                errs() << "Warning: fortran names may not start with an underscore. Macro argument ";
                errs() << argname << " renamed h2m" << argname << "\n";
                LineError(sloc);
              }
              argname = "h2m" + argname;  // Fix the problem by prepending h2m
            }
            fortranMacro += argname;
            fortranMacro += ", ";
          }
          // erase the redundant colon
          fortranMacro.erase(fortranMacro.size()-2);
          fortranMacro += ") bind (C)\n";
          // Check that this line is not too long. Take into account the fact that the
          // characters INTERFACE\n alleady at the begining take up 10 characters and the 
          // newline just added uses another one.
          CheckLength(fortranMacro, CToFTypeFormatter::line_max + 11, args.getSilent(), sloc);

        }
        if (!functionBody.empty()) {
          std::istringstream in(functionBody);
          for (std::string line; std::getline(in, line);) {
            if (args.getSilent() == false && args.getQuiet() == false) {
              errs() << "Warning: line " << line << " commented out.\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        fortranMacro += "END SUBROUTINE " + macroName + "\n";
        fortranMacro += "END INTERFACE\n";
      }
    }

    // Here checks for illegal name lengths and repeated names occur. It seemed best to do this
    // in one place. I must surrender to the strange structure of this function already in place.
    string temp_name;
    if (macroName.front() == '_') {
      temp_name = "h2m" + macroName;
    } else {
      temp_name = macroName;
    }
    // Check the name's length to make sure that it is valid
    CheckLength(temp_name, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Now check to see if this is a repeated identifier. This is very uncommon but could occur.
    if (RecordDeclFormatter::StructAndTypedefGuard(temp_name) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << temp_name << ", macro definition.\n";
        LineError(sloc);
      }
      string temp_buf = fortranMacro;  // Comment out all the lines using an istringstream
      fortranMacro = "\n! Duplicate declaration of " + temp_name + ", MACRO, skipped.\n";
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        fortranMacro += "! " + line + "\n";
      }
    }
    

  }
  return fortranMacro;
};



//-----------AST visit functions----------------------------------------------------------------------------------------------------

// This function does the work of determining what the node currently under traversal
// is and creating the proper object-translation object to handle it.
bool TraverseNodeVisitor::TraverseDecl(Decl *d) {
  if (isa<TranslationUnitDecl> (d)) {
    // tranlastion unit decl is the top node of all AST, ignore the inner structure of tud for now
    RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d);

  } else if (isa<FunctionDecl> (d)) {
    // Keep included header files out of the mix by checking the location
    // Includes become part of the AST. However, there shouldn't be any
    // problem with potentially invalid locations during the check. The
    // Function checks for that.
    // Note that, if the option Together is specified, the entire AST (save
    // for system headers which are checked for elsewhere) is sent to the
    // same file due to the || statement
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      FunctionDeclFormatter fdf(cast<FunctionDecl> (d), TheRewriter, args);
      allFunctionDecls += fdf.getFortranFunctDeclASString();
    }
    
    // args.getOutput().os() << "INTERFACE\n" 
    // << fdf.getFortranFunctDeclASString()
    // << "END INTERFACE\n";      
  } else if (isa<TypedefDecl> (d)) {
    // Keep included header files out of the mix by checking the location
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      TypedefDecl *tdd = cast<TypedefDecl> (d);
      TypedefDeclFormater tdf(tdd, TheRewriter, args);
      args.getOutput().os() << tdf.getFortranTypedefDeclASString();
    }

  } else if (isa<RecordDecl> (d)) {
    // Keep included header files out of the mix by checking the location
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      RecordDecl *rd = cast<RecordDecl> (d);
      RecordDeclFormatter rdf(rd, TheRewriter, args);
      args.getOutput().os() << rdf.getFortranStructASString();
    }

  } else if (isa<VarDecl> (d)) {
    // Keep included header files out of the mix by checking the location
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      VarDecl *varDecl = cast<VarDecl> (d);
      VarDeclFormatter vdf(varDecl, TheRewriter, args);
      args.getOutput().os() << vdf.getFortranVarDeclASString();
    } 

  } else if (isa<EnumDecl> (d)) {
    // Keep included header files out of the mix by checking the location
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      EnumDeclFormatter edf(cast<EnumDecl> (d), TheRewriter, args);
      args.getOutput().os() << edf.getFortranEnumASString();
    }
  } else {

    // Keep included header files out of the mix by checking the location
    if (TheRewriter.getSourceMgr().isInMainFile(d->getLocation()) ||
        args.getTogether() == true) {
      args.getOutput().os() << "!found other type of declaration \n";
      d->dump();
      RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d);
    }
  }
    // comment out because function declaration doesn't need to be traversed.
    // RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d); // Forward to base class

    return true; // Return false to stop the AST analyzing

};


// Currently, there are no attempts made to traverse and translate statements into 
// Fortran. This method simply comments out statements and warns about them if
// necessary.
bool TraverseNodeVisitor::TraverseStmt(Stmt *x) {
  string stmtText;
  string stmtSrc = Lexer::getSourceText(CharSourceRange::getTokenRange(x->getLocStart(), x->getLocEnd()), TheRewriter.getSourceMgr(), LangOptions(), 0);
  // comment out stmtText
  std::istringstream in(stmtSrc);
  for (std::string line; std::getline(in, line);) {
    // Output warnings about commented out statements only if a loud run is in progress.
    if (args.getQuiet() == false && args.getSilent() == false) {
      errs() << "Warning: statement " << stmtText << " commented out.\n";
      LineError(TheRewriter.getSourceMgr().getPresumedLoc(x->getLocStart()));
    }
    stmtText += "! " + line + "\n";
  }
  args.getOutput().os() << stmtText;

  RecursiveASTVisitor<TraverseNodeVisitor>::TraverseStmt(x);
  return true;
};
// Currently, there are no attempts made to traverse and translate types 
// into Fotran. This method simply comments out the declaration.
bool TraverseNodeVisitor::TraverseType(QualType x) {
  string qt_string = "!" + x.getAsString();
  args.getOutput().os() << qt_string;
  if (args.getQuiet() == false && args.getSilent() == false) { 
    errs() << "Warning: type " << qt_string << " commented out.\n";
  }
  RecursiveASTVisitor<TraverseNodeVisitor>::TraverseType(x);
  return true;
};

//---------------------------Main Program Class Functions---------
void TraverseMacros::MacroDefined (const Token &MacroNameTok, const MacroDirective *MD) {
    MacroFormatter mf(MacroNameTok, MD, ci, args);
    args.getOutput().os() << mf.getFortranMacroASString();
}

// HandlTranslationUnit is the overarching entry into the clang ast which is
// called to begin the traversal.
void TraverseNodeConsumer::HandleTranslationUnit(clang::ASTContext &Context) {
// Traversing the translation unit decl via a RecursiveASTVisitor
// will visit all nodes in the AST.

  Visitor.TraverseDecl(Context.getTranslationUnitDecl());

  // wrap all func decls in a single interface
  if (!Visitor.allFunctionDecls.empty()) {
    args.getOutput().os() << "INTERFACE\n" 
    << Visitor.allFunctionDecls
    << "END INTERFACE\n";   
  }
}

// Executed when each source begins. This allows the boiler plate required for each
// module to be added to the file.
bool TraverseNodeAction::BeginSourceFileAction(CompilerInstance &ci, StringRef Filename)
{
  fullPathFileName = Filename;
  // We have to pass this back out to keep track of repeated module names
  args.setModuleName(GetModuleName(Filename, args));

  // initalize Module and imports
  string beginSourceModule;
  beginSourceModule = "MODULE " + args.getModuleName() + "\n";
  beginSourceModule += "USE, INTRINSIC :: iso_c_binding\n";
  beginSourceModule += use_modules;
  beginSourceModule += "implicit none\n";
  args.getOutput().os() << beginSourceModule;

  Preprocessor &pp = ci.getPreprocessor();
  pp.addPPCallbacks(llvm::make_unique<TraverseMacros>(ci, args));
  return true;
}

// Executed when a source file is finished. This allows the boiler plate required for the end of
// a fotran module to be added to the file.
void TraverseNodeAction::EndSourceFileAction() {
    //Now emit the rewritten buffer.
    //TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(args.getOutput().os());

    string endSourceModle;
    endSourceModle = "END MODULE " + args.getModuleName() + "\n";
    args.getOutput().os() << endSourceModle;
  }

int main(int argc, const char **argv) {
  if (argc > 1) {
    // Parse the command line options and create a new database to hold the Clang compilation
    // options.
    cl::HideUnrelatedOptions(h2mOpts); // This hides all the annoying clang options in help output
    cl::ParseCommandLineOptions(argc, argv, "h2m Autofortran Tool\n");
    std::unique_ptr<CompilationDatabase> Compilations;
    SmallString<256> PathBuf;
    sys::fs::current_path(PathBuf);
    Compilations.reset(new FixedCompilationDatabase(Twine(PathBuf), other));


    // Checks for illegal options. Give warnings, and errors.
    if (IgnoreThis == true && Recursive == false) {  // Don't translate this, and no recursion requested
      errs() << "Error: incompatible options, skip given file and no recursion (-i without -r)";
      errs() << "Either specify a recursive translation or remove the option to skip the first file.";
      return(1);
    // If all the main file's AST (including headers courtesty of the preprocessor) is sent to one
    // module and a recursive option is also specified, things defined in the headers will be
    // defined multiple times over several recursively translated modules.
    } else if (Together == true && Recursive == true) {
      errs() << "Warning: request for all local includes to be sent to a single file accompanied\n";
      errs() << "by recursive translation (-t and -r) may result in multiple declaration errors.\n";
    }
    // Determine file to open and initialize it. Wrtie to stdout if no file is given.
    string filename;
    std::error_code error;
    if (OutputFile.size()) {
      filename = OutputFile;
    } else {
      filename = "-";
    }
    // The file is opened in text mode
    llvm::tool_output_file OutputFile(filename, error, llvm::sys::fs::F_Text);
    if (error) {  // Error opening file
      errs() << "Error opening output file: " << filename << error.message() << "\n";
      return(1);  // We can't possibly keep going if the file can't be opened.
    }
    if (Optimistic == true) {  // Keep all output inspite of errors
      OutputFile.keep();
    }
    // Create an object to pass around arguments
    Arguments args(Quiet, Silent, OutputFile, NoHeaders, Together, Transpose);


    // Create a new clang tool to be used to run the frontend actions
    ClangTool Tool(*Compilations, SourcePaths);
    int tool_errors = 0;  // No errors have occurred running the tool yet


    // Write some initial text into the file, just boilerplate stuff.
    OutputFile.os() << "! The following Fortran code was generated by the h2m-Autofortran ";
    OutputFile.os() << "Tool.\n! See the h2m README file for credits and help information.\n\n";

    // Follow the preprocessor's inclusions to generate a recursive 
    // order of hearders to be translated and linked by "USE" statements
    if (Recursive) {
      std::set<string> seenfiles;
      std::stack<string> stackfiles;
      // CHS means "CreateHeaderStack." The seenfiles is a set used to keep track of what 
      // has been seen, and the stackfiles is used to keep track of the order to include them.
      CHSFrontendActionFactory CHSFactory(seenfiles, stackfiles, args);
      int initerrs = Tool.run(&CHSFactory);  // Run the first action to follow inclusions
      // If the attempt to find the needed order to translate the headers fails,
      // this effort is probably doomed.
      if (initerrs != 0) {
        errs() << "Error during preprocessor-tracing tool run, errno ." << initerrs << "\n";
        if (Optimistic == false) {  // Exit unless told to keep going
          errs() << "A non-recursive run may succeed.\n";
          errs() << "Alternately, enable optimistic mode (-keep-going or -k) to continue despite errors.\n";
          return(initerrs);
        } else if (stackfiles.empty() == true) {  // Whatever happend is not recoverable.
          errs() << "Unrecoverable initialization error. No files recorded to translate.\n";
          return(initerrs);
        } else {
          errs() << "Optimistic run continuing.\n";
        }
      }

      // Dig through the created stack of header files we have seen, as prepared by
      // the first Clang tool which tracks the preprocessor. Translate them each in turn.
      string modules_list;
      while (stackfiles.empty() == false) {
        string headerfile = stackfiles.top();
        stackfiles.pop(); 

        if (stackfiles.empty() && IgnoreThis == true) {  // We were directed to skip the first file
          break;  // Leave the while loop. This is cleaner than another level of indentation.
        }

        ClangTool stacktool(*Compilations, headerfile);  // Create a tool to run on each file in turn
        TNAFrontendActionFactory factory(modules_list, args);
        // modules_list is the growing string of previously translated modules this module may depend on
        // Run the translation tool.
        tool_errors = stacktool.run(&factory);

        if (tool_errors != 0) {  // Tool error occurred
          if (Silent == false) {  // Do not report the error if the run is silent.
            errs() << "Translation error occured on " << headerfile;
            errs() <<  ". Output may be corrupted or missing.\n";
            errs() << "\n\n\n\n";  // Put four lines between files to help keep track of errors
          }
          // Comment out the use statement becuase the module may be corrupt.
          modules_list += "!USE " + args.getModuleName() + "\n";
          OutputFile.os()  << "! Warning: Translation Error Occurred on this module\n";
        } else {  // Successful run, no errors
          // Add USE statement to be included in future modules
          modules_list += "USE " + args.getModuleName() + "\n";
          if (Silent == false) {  // Don't clutter the screen if the run is silent
            errs() << "Successfully processed " << headerfile << "\n";
            errs() << "\n\n\n\n";  // Put four lines between files to help keep track of errors
          }
        }
        args.setModuleName("");  // For safety, unset the module name passed out of Arguments

        args.getOutput().os() << "\n\n";  // Put two lines inbetween modules, even on a trans. failure
      }  // End looking through the stack and processing all headers (including the original).

    } else {  // No recursion, just run the tool on the first input file. No module list string is needed.
      TNAFrontendActionFactory factory("", args);
      tool_errors = Tool.run(&factory);
    }  // End processing of the translation

    // Note that the output has already been kept if this is an optimistic run. It doesn't hurt to keep twice.
    if (!tool_errors) {  // If the last run of the tool was not successful, the output may be garbage.
      OutputFile.keep();
    }

    // A compiler post-process has been specified and there is an actual output file,
    // prepare and run the compiler.
    if (Compiler.size() && filename.compare("-") != 0) {
      OutputFile.os().flush();  // Attempt to flush the stream to avoid a partial read by the compiler
      if (system(NULL) == true) {  // A command interpreter is available
        string command = Compiler + " " + filename;
        int success = system(command.c_str());
	if (Silent == false) {  // Notify if the run is noisy
	  if (success == 0) {  // Successful compilation.
	    errs() << "Successful compilation of " << filename << " with " << Compiler << "\n";
	  } else {  // Inform of the error and give the error number
	    errs() << "Unsuccessful compilation of " << filename << " with ";
	    errs() << Compiler << ". Error: " << success << "\n";
	  } 
        }
      } else {  // Cannot run using system (fork might succeed but is very error prone).
        errs() << "Error: No command interpreter available to run system process " << Compiler << "\n";
      }
    // We were asked to run the compiler, but there is no output file, report an error.
    } else if (Compiler.size() && filename.compare("-") == 0) {
      errs() << "Error: unable to attempt compilation on standard output.\n";
    }
    return(tool_errors);
  }

  errs() << "At least one argument (header to process) must be provided.\n";
  errs() << "Run 'h2m -help' for usage details.\n";
  return(1);  // Someone did not give any arguments
};
