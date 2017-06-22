//stands for header to module
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

// Fetches the fortran module name from a given filepath Filename
// IMPORTANT: ONLY call this ONCE for ANY FILE. The static 
// structure requires this, otherwise you will have all sorts of
// bizarre problems.
static string GetModuleName(string Filename, Arguments& args) {
  static std::map<string, int> repeats;
  size_t slashes = Filename.find_last_of("/\\");
  string filename = Filename.substr(slashes+1);
  size_t dot = filename.find('.');
  filename = filename.substr(0, dot);
  if (repeats.count(filename) > 0) {  // We have found a repeated module name
    int append = repeats[filename];
    repeats[filename] = repeats[filename] + 1;  // Record the new repeat in the map
    string oldfilename = filename;  // Used to report the error, no other purpose
    filename = filename + "_" + std::to_string(append);  // Add _# to the end of the name
    if (args.getSilent() == false) {
      errs() << "Warning: repeated module name module_" << oldfilename << " found, source is " << Filename;
      errs() << ". Name changed to module_" << filename << "\n";
    }
  } else {  // Record the first appearance of this module name
    repeats[filename] = 2;
  }
  filename = "module_" + filename;
  return filename;
}

// Command line options:

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory h2mOpts("Options for the h2m translator.");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

// Positional parameter: the first input parameter should be the compilation files
static cl::opt<string> SourcePaths(cl::Positional, cl::desc("source to translate"));

// Output file, option is out
static cl::opt<string> OutputFile("out", cl::init(""), cl::desc("Output file"));
static cl::alias OutputFile2("o", cl::desc("Alias for the output file"), cl::aliasopt(OutputFile));

// Boolean option to recursively process includes
static cl::opt<bool> Recursive("recursive", cl::desc("Include other header files recursively via USE statements"));
static cl::alias Recrusive2("r", cl::desc("Alias for recursive"), cl::aliasopt(Recursive));

// Boolean option to silence less critical warnings
static cl::opt<bool> Quiet("quiet", cl::desc("Silence warnings about lines which have been commented out."));
static cl::alias Quiet2("q", cl::desc("Alias for quiet"), cl::aliasopt(Quiet));

// Boolean option to silence all warnings save those that are absolutely imperative (ie output failure)
static cl::opt<bool> Silent("silent", cl::desc("Silence all tool warnings. Clang warnings will still appear."));
static cl::alias Silent2("s", cl::desc("Alias for silent"), cl::aliasopt(Silent));

// Boolean option to ignore critical clang errors that would otherwise cause termination
static cl::opt<bool> Optimistic("optimist", cl::desc("Continue processing and keep output in spite of errors"));
static cl::alias Optimistic2("k", cl::desc("Alias for optimist"), cl::aliasopt(Optimistic));

// Boolean option to ignore system header files when processing recursive includes
static cl::opt<bool> NoHeaders("no-system-headers", cl::desc("Do not recursively translate system header files."));
static cl::alias NoHeaders2("n", cl::desc("Alias for no-system-headers"), cl::aliasopt(NoHeaders));

// Option to specify the compiler to use to test the output. No specification means no compilation.
static cl::opt<string> Compiler("compile", cl::desc("Program to be used to attempt to compile the output file."));
static cl::alias Compiler2("c", cl::desc("Alias for compile"), cl::aliasopt(Compiler));

static cl::opt<string> other(cl::ConsumeAfter, cl::desc("Front end arguments"));


// -----------initializer RecordDeclFormatter--------------------
CToFTypeFormatter::CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc loc, Arguments &arg): ac(ac), args(arg) {
  c_qualType = qt;
  sloc = loc;
  // sloc = ac.getSourceManager().getPresumedLoc(TypeLoc(qt, qt.getAsOpaquePtr()).getEndLoc()); // # TODO: WHY DOESN'T THIS WORK?
};

// Determines whether the qualified type offered is identical to that it is called on.
// Pointer types are only distinguished in terms of function vs data pointers. 
bool CToFTypeFormatter::isSameType(QualType qt2) {
  // for pointer type, only distinguish between the function pointer from other pointers
  if (c_qualType.getTypePtr()->isPointerType() and qt2.getTypePtr()->isPointerType()) {
    if (c_qualType.getTypePtr()->isFunctionPointerType() and qt2.getTypePtr()->isFunctionPointerType()) {
      return true;
    } else if ((!c_qualType.getTypePtr()->isFunctionPointerType()) and (!qt2.getTypePtr()->isFunctionPointerType())) {
      return true;
    } else {
      return false;
    }
  } else {
    return c_qualType == qt2;
  }
};

// Typically this returns the raw id, but in the case of an array, it must 
// determine an array suffix by determining the type of the array, the element
// size, and the length in order to provide a declaration.
string CToFTypeFormatter::getFortranIdASString(string raw_id) {
  // Determine if it needs to be substituted out
  if (c_qualType.getTypePtr()->isArrayType()) {
    const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
    QualType e_qualType = at->getElementType ();
    int typeSize = ac.getTypeSizeInChars(c_qualType).getQuantity();
    int elementSize = ac.getTypeSizeInChars(e_qualType).getQuantity();
    int numOfEle = typeSize / elementSize;
    string arr_suffix = "(" + to_string(numOfEle) + ")";
    raw_id += arr_suffix;
  }
  return raw_id;
};

// The type in C is found in the c_qualType variable. The type is 
// determined over a series of if statements and a suitable Fortran 
// equivalent is returned as a string. The boolean typeWrapper dictates
// whether or not the return should be of the form FORTRANTYPE(C_QUALIFIER)
// or just of the form C_QUALIFIER.
string CToFTypeFormatter::getFortranTypeASString(bool typeWrapper) {
  string f_type;

      // char
  if (c_qualType.getTypePtr()->isCharType()) {
    if (typeWrapper) {
      f_type = "CHARACTER(C_CHAR)";
    } else {
      f_type = "C_CHAR";
    }
  } else if (c_qualType.getTypePtr()->isBooleanType()) {
    if (typeWrapper) {
      f_type = "LOGICAL(C_BOOL)";
    } else {
      f_type = "C_BOOL";
    }
    // INT
  } else if (c_qualType.getTypePtr()->isIntegerType()) {
    //int typeSize = ac.getTypeSizeInChars(c_qualType).getQuantity();
    // diff int type may have same size so use stirng matching for now
     if (c_qualType.getAsString()== "size_t") {
      // size_t
      if (typeWrapper) {
        f_type = "INTEGER(C_SIZE_T)";
      } else {
        f_type = "C_SIZE_T";
      }
    } else if (c_qualType.getAsString()== "unsigned char" or c_qualType.getAsString()== "signed char") {
      // signed/unsigned char
      if (typeWrapper) {
        f_type = "INTEGER(C_SIGNED_CHAR)";
      } else {
        f_type = "C_SIGNED_CHAR";
      }        
    } else if (c_qualType.getAsString().find("short") != std::string::npos) {
      // short
      if (typeWrapper) {
        f_type = "INTEGER(C_SHORT)";
      } else {
        f_type = "C_SHORT";
      }  
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
      // long or long long, assume long
      if (typeWrapper) {
        f_type = "INTEGER(C_LONG)";
      } else {
        f_type = "C_LONG";
      }      
    } else {
      // other int so just assume int
      if (typeWrapper) {
        f_type = "INTEGER(C_INT)";
      } else {
        f_type = "C_INT";
      }      
    }

    // REAL
  } else if (c_qualType.getTypePtr()->isRealType()) {

    if (c_qualType.getAsString().find("long") != std::string::npos) {
      // long double
      if (typeWrapper) {
        f_type = "REAL(C_LONG_DOUBLE)";
      } else {
        f_type = "C_LONG_DOUBLE";
      } 
    } else if (c_qualType.getAsString()== "float") {
      // float
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT)";
      } else {
        f_type = "C_FLOAT";
      }
    } else if (c_qualType.getAsString()== "__float128") {
      // __float128
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT128)";
      } else {
        f_type = "C_FLOAT128";
      }
    } else {
      // should be double
      if (typeWrapper) {
        f_type = "REAL(C_DOUBLE)";
      } else {
        f_type = "C_DOUBLE";
      }      
    }
    // COMPLEX
  } else if (c_qualType.getTypePtr()->isComplexType ()) {
    if (c_qualType.getAsString().find("float") != std::string::npos) {
      //  float _Complex 
      if (typeWrapper) {
        f_type = "COMPLEX(C_FLOAT_COMPLEX)";
      } else {
        f_type = "C_FLOAT_COMPLEX";
      }        
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
       //  long double _Complex 
      if (typeWrapper) {
        f_type = "COMPLEX(C_LONG_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_LONG_DOUBLE_COMPLEX";
      }
    } else {
      // assume double _Complex 
      if (typeWrapper) {
        f_type = "COMPLEX(C_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_DOUBLE_COMPLEX";
      }
    }
    // POINTER
  } else if (c_qualType.getTypePtr()->isPointerType ()) {
    if (c_qualType.getTypePtr()->isFunctionPointerType()){
      if (typeWrapper) {
        f_type = "TYPE(C_FUNPTR)";
      } else {
        f_type = "C_FUNPTR";
      }
    } else {
      if (typeWrapper) {
        f_type = "TYPE(C_PTR)";
      } else {
        f_type = "C_PTR";
      }
    }
    
  } else if (c_qualType.getTypePtr()->isStructureType()) {
    // struct type
    f_type = c_qualType.getAsString();
    // We cannot have a space in a fortran type. Erase up
    // to the space.
    size_t found = f_type.find_first_of(" ");
    while (found != string::npos) {
      f_type.erase(0, found);  // Erase up to the final space
      found=f_type.find_first_of(" ",found+1);
    }
    if (f_type.front() == '_') {
      if (args.getQuiet() == false && args.getSilent() == false) {
        errs() << "Warning: fortran names may not begin with an underscore.";
        errs() << f_type << " renamed " << "h2m" << f_type << "\n";
        LineError(sloc);
      }
      f_type = "h2m" + f_type;  // Prepend h2m to fix the naming problem
    }
    if (typeWrapper) {
      f_type = "TYPE(" + f_type + ")";
    } 
    // ARRAY
  } else if (c_qualType.getTypePtr()->isArrayType()) {
    const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
    // recursively get element type
    QualType e_qualType = at->getElementType ();
    CToFTypeFormatter etf(e_qualType, ac, sloc, args);

    f_type = etf.getFortranTypeASString(typeWrapper);
  } else {
    f_type = "unrecognized_type(" + c_qualType.getAsString()+")";
    // Avoids repetitive error messages
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
// Useful in determining how to translate a C macro.
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
// double. Useful for determining how to translate a C macro.
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
// for determining how to translate a C macro.
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
// how to translate a C macro.
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
// Useful for determining how to translate a C macro etc.
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

// Retrurns a string buffer containing the Fortran equivalent of a C macro resembling a typedef.
// The argumnets and PresumedLoc are used to give information about the location of any
// errors that might occur during an attempted translation. The macro is translated into 
// a Fortran TYPE.
string CToFTypeFormatter::createFortranType(const string macroName, const string macroVal, PresumedLoc loc, Arguments &args) {
  string ft_buffer;
  string type_id = "typeID_" + macroName ;
  
  // Create a new name for the transalated type.
  // replace space with underscore 
  size_t found = type_id.find_first_of(" ");
  while (found != string::npos) {
    type_id[found] = '_';
    found=type_id.find_first_of(" ",found+1);
  }
  if (args.getQuiet() == false && args.getSilent() == false) {
    errs() << "Warning: Translation of typedef like macro requires renaming ";
    errs() << macroName << type_id << "\n";
    LineError(loc);
  }

  if (macroName[0] == '_') {
    if (args.getSilent() == false) {
      errs() << "Warning: Fortran names may not start with '_' ";
      errs() << macroName << " renamed h2m" << macroName << "\n";
      LineError(loc);
    }
    ft_buffer += "TYPE, BIND(C) :: h2m" + macroName+ "\n";
    if (macroVal.find("char") != std::string::npos) {
      ft_buffer += "    CHARACTER(C_CHAR) :: " + type_id + "\n";
    } else if (macroVal.find("long") != std::string::npos) {
      ft_buffer += "    INTEGER(C_LONG) :: " + type_id + "\n";
    } else if (macroVal.find("short") != std::string::npos) {
      ft_buffer += "    INTEGER(C_SHORT) :: " + type_id + "\n";
    } else {
      ft_buffer += "    INTEGER(C_INT) :: " + type_id + "\n";
    }
    ft_buffer += "END TYPE h2m" + macroName+ "\n";
  } else {
    ft_buffer = "TYPE, BIND(C) :: " + macroName+ "\n";
    if (macroVal.find("char") != std::string::npos) {
      ft_buffer += "    CHARACTER(C_CHAR) :: " + type_id + "\n";
    } else if (macroVal.find("long") != std::string::npos) {
      ft_buffer += "    INTEGER(C_LONG) :: " + type_id + "\n";
    } else if (macroVal.find("short") != std::string::npos) {
      ft_buffer += "    INTEGER(C_SHORT) :: " + type_id + "\n";
    } else {
      ft_buffer += "    INTEGER(C_INT) :: " + type_id + "\n";
    }
    ft_buffer += "END TYPE " + macroName+ "\n";
  }
  return ft_buffer;
};

// -----------initializer VarDeclFormatter--------------------
VarDeclFormatter::VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  varDecl = v;
  isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(varDecl->getSourceRange().getBegin());
  sloc = rewriter.getSourceMgr().getPresumedLoc(varDecl->getSourceRange().getBegin());
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
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc (), varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
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
      if (args.getSilent() == false) {
        errs() << "Variable declaration initialization commented out:\n" << valString << " \n";
        LineError(sloc);
      }
    }
  }
  return valString;

};

// This function fetches the type and name of an array element through the 
// ast context.
void VarDeclFormatter::getFortranArrayEleASString(InitListExpr *ile, string &arrayValues, string arrayShapes, bool &evaluatable, bool firstEle) {
  ArrayRef<Expr *> innerElements = ile->inits ();
  if (firstEle) {
    size_t numOfInnerEle = innerElements.size();
    arrayShapes += ", " + to_string(numOfInnerEle);
    arrayShapes_fin = arrayShapes;
  }
  for (auto it = innerElements.begin (); it != innerElements.end(); it++) {
    Expr *innerelement = (*it);
    if (innerelement->isEvaluatable (varDecl->getASTContext())) {
      evaluatable = true;
      clang::Expr::EvalResult r;
      innerelement->EvaluateAsRValue(r, varDecl->getASTContext());
      string eleVal = r.Val.getAsString(varDecl->getASTContext(), innerelement->getType());
      if (arrayValues.empty()) {
        arrayValues = eleVal;
      } else {
        arrayValues += ", " + eleVal;
      }
    } else if (isa<InitListExpr> (innerelement)) {
      InitListExpr *innerile = cast<InitListExpr> (innerelement);
      getFortranArrayEleASString(innerile, arrayValues, arrayShapes, evaluatable, (it == innerElements.begin()) and firstEle);
    } 
  }
};

// Much more complicated function used to generate an array declaraion. 
string VarDeclFormatter::getFortranArrayDeclASString() {
  string arrayDecl;
  if (varDecl->getType().getTypePtr()->isArrayType() and !isInSystemHeader) {
    CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
    if (!varDecl->hasInit()) {
      // only declared, no init
      arrayDecl = tf.getFortranTypeASString(true) + ", public :: " + tf.getFortranIdASString(varDecl->getNameAsString()) + "\n";
    } else {
      // has init
      const ArrayType *at = varDecl->getType().getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType ();
      if (e_qualType.getTypePtr()->isCharType()) {
        // handle stringliteral case
        Expr *exp = varDecl->getInit();
        string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc (), varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
        arrayDecl = tf.getFortranTypeASString(true) + ", parameter, public :: " + tf.getFortranIdASString(varDecl->getNameAsString()) + " = " + arrayText + "\n";
      } else {
        bool evaluatable = false;
        Expr *exp = varDecl->getInit();
        if (isa<InitListExpr> (exp)) {
          // initialize shape and values
          // format: INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/))
          string arrayValues;
          string arrayShapes;

          InitListExpr *ile = cast<InitListExpr> (exp);
          ArrayRef< Expr * > elements = ile->inits ();
          size_t numOfEle = elements.size();
          arrayShapes = to_string(numOfEle);
          arrayShapes_fin = arrayShapes;
          for (auto it = elements.begin (); it != elements.end(); it++) {
            Expr *element = (*it);
            if (isa<InitListExpr> (element)) {
                  // multidimensional array
              InitListExpr *innerIle = cast<InitListExpr> (element);
              getFortranArrayEleASString(innerIle, arrayValues, arrayShapes, evaluatable, it == elements.begin());
            } else {
              if (element->isEvaluatable (varDecl->getASTContext())) {
                    // one dimensional array
                clang::Expr::EvalResult r;
                element->EvaluateAsRValue(r, varDecl->getASTContext());
                string eleVal = r.Val.getAsString(varDecl->getASTContext(), e_qualType);

                if (it == elements.begin()) {
                      // first element
                  evaluatable = true;
                  arrayValues = eleVal;
                } else {
                  arrayValues += ", " + eleVal;
                }

              }
            }
          } //<--end iteration
          if (!evaluatable) {
            string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(varDecl->getSourceRange()), rewriter.getSourceMgr(), LangOptions(), 0);
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
            arrayDecl += tf.getFortranTypeASString(true)+" :: "+ varDecl->getNameAsString() +"("+arrayShapes_fin+")"+" = RESHAPE((/"+arrayValues+"/), (/"+arrayShapes_fin+"/))\n";

          }
        }
      }     
    }
  }
  return arrayDecl;
};



// Hanldes various variable declarations and returns them as fortran strings. Helper
// functions are called for arrays which are difficult to format. The type is 
// fetched with the hlper function getFortranTypeASString. Any illegal identifiers (ie _thing)
// prepended with "h2m" to become legal fortran identifiers.
string VarDeclFormatter::getFortranVarDeclASString() {
  string vd_buffer;
  
  if (!isInSystemHeader) {
    if (varDecl->getType().getTypePtr()->isStructureType()) {
      // structure type
      RecordDecl *rd = varDecl->getType().getTypePtr()->getAsStructureType()->getDecl();
      RecordDeclFormatter rdf(rd, rewriter, args);
      rdf.setTagName(varDecl->getNameAsString());  // TODO: Experiment with this line
      vd_buffer = rdf.getFortranStructASString();
    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
        // handle initialized numeric array specifically
        vd_buffer = getFortranArrayDeclASString();
    } else if (varDecl->getType().getTypePtr()->isPointerType() and 
      varDecl->getType().getTypePtr()->getPointeeType()->isCharType()) {
      // string declaration
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType().getTypePtr()->getPointeeType(), varDecl->getASTContext(), sloc, args);
      string identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      if (identifier.front() == '_') {
         if (args.getSilent() == false && args.getQuiet() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         identifier = "h2m" + identifier;
      }
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public :: " + identifier + " " + value + "\n";
      } else {
        vd_buffer = tf.getFortranTypeASString(true) + ", parameter, public :: " + identifier + " = " + value + "\n";
      }
    } else {
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      string identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      if (identifier.front() == '_') {
         if (args.getSilent() == false && args.getQuiet() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         identifier = "h2m" + identifier;
      } 
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public :: " + identifier + " " + value + "\n";
      } else {
        vd_buffer = tf.getFortranTypeASString(true) + ", parameter, public :: " + identifier + " = " + value + "\n";
      }
    }
  }

  return vd_buffer;
};

// -----------initializer Typedef--------------------
TypedefDeclFormater::TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  typedefDecl = t;
  isLocValid = typedefDecl->getSourceRange().getBegin().isValid();
  if (isLocValid) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(typedefDecl->getSourceRange().getBegin());
  }  
  sloc = rewriter.getSourceMgr().getPresumedLoc(typedefDecl->getSourceRange().getBegin());
  
};

// From a C typedef, a string representing a Fortran typedef is created. The fortran equivalent
// is a type with only one field. The name of this field is name_C_INT or name_C_PTR, depending
// on the type. Note that this function will be called for Structs and Enums as well, but that they
// will be skipped and handled elsewhere (in recordDeclFormatter). A typedef with an illegal 
// name will be prepended with "h2m" to become legal fortran.
string TypedefDeclFormater::getFortranTypedefDeclASString() {
  string typdedef_buffer;
  if (isLocValid and !isInSystemHeader) {
      if (typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isStructureType()
        or typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isEnumeralType ()) {
        // skip it, since it will be translated in recordecl
      } else {
        // other regular type defs
        TypeSourceInfo * typeSourceInfo = typedefDecl->getTypeSourceInfo();
        CToFTypeFormatter tf(typeSourceInfo->getType(), typedefDecl->getASTContext(), sloc, args);
        string identifier = typedefDecl->getNameAsString();
        if (identifier.front() == '_') {  // This identifier has an illegal _ at the begining!
          string old_identifier = identifier;
          identifier = "h2m" + identifier;  // Prepend h2m to fix the problem
          if (args.getQuiet() == false && args.getSilent() == false) {  // Warn unless silenced
            errs() << "Warning: illegal identifier " << old_identifier << " renamed " << identifier << "\n";
            LineError(sloc);
          }
        }
        typdedef_buffer = "TYPE, BIND(C) :: " + identifier + "\n";
        // TODO: DECIDE IF THIS IS WHAT YOU ACTUALLY WANT
        // Because names in structs may collide with the struct name, suffixes are appended
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: due to name collisions during typdef translation, " << identifier;
          errs() <<  "\nrenamed " << identifier << tf.getFortranTypeASString(false) << "\n";
          LineError(sloc);
        }
        typdedef_buffer += "    "+ tf.getFortranTypeASString(true) + "::" + identifier+"_"+tf.getFortranTypeASString(false) + "\n";
        typdedef_buffer += "END TYPE " + identifier + "\n";
      }
    } 
  return typdedef_buffer;
};

// -----------initializer EnumDeclFormatter--------------------
EnumDeclFormatter::EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  enumDecl = e;
  isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(enumDecl->getSourceRange().getBegin());
  sloc = rewriter.getSourceMgr().getPresumedLoc(enumDecl->getSourceRange().getBegin());
};

// From a C enumerated type, a fotran ENUM is created. The names are prepended
// with h2m if they begin with an underscore. The function loops through all 
// the members in the enumerator and adds them into place. Note that this can
// resut in serious problems if the enumeration is too large.
string EnumDeclFormatter::getFortranEnumASString() {
  string enum_buffer;
  if (!isInSystemHeader) {  // I have no idea why this is here. It doesn't seem to be doing anything.
    string enumName = enumDecl->getNameAsString();
    enum_buffer = "ENUM, BIND( C )\n";
    // enum_buffer += "    enumerator :: ";  // Removed when changes were made to allow unlimited enum length
    // Cycle through the pieces of the enum and translate them into fortran
    for (auto it = enumDecl->enumerator_begin (); it != enumDecl->enumerator_end (); it++) {
      string constName = (*it)->getNameAsString ();
      if (constName.front() == '_') {  // The name begins with an illegal underscore.
        string old_constName = constName;
        constName = "h2m" + constName;
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: illegal enumeration identfier " << old_constName << " renamed ";
          errs() << constName << "\n";
          LineError(sloc);
        }
      }
      int constVal = (*it)->getInitVal ().getExtValue ();  // Get the initialization value
      enum_buffer += "    enumerator :: " + constName + "=" + to_string(constVal) + "\n";
    }
    // erase the redundant colon  // This erasing and adding back in of a newline is obsolete with the new format
    // enum_buffer.erase(enum_buffer.size()-2);
    // enum_buffer += "\n";
    // Put the actual name of the enumerator as an uninitialized member of the enumerated type
    if (!enumName.empty()) {
      if (enumName.front() == '_') {  // Illegal underscore beginning the name!
        string old_enumName = enumName;
        enumName = "h2m" + enumName;  // Prepend h2m to fix the problem
        if (args.getSilent() == false && args.getQuiet() == false) {  // Warn unless silenced
          errs() << "Warning: illegal enumeration identifier " << old_enumName << " renamed " << enumName << "\n";
          LineError(sloc); 
        } 
      }
      enum_buffer += "    enumerator " + enumName+"\n";
    } else {  // I don't think this is actually a problem for potential illegal underscores.
      string identifier = enumDecl-> getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.find("anonymous at") == string::npos) {
        enum_buffer += "    enumerator " + identifier+"\n";
      }
    }

      enum_buffer += "END ENUM\n";
  }

  return enum_buffer;
};

// -----------initializer RecordDeclFormatter--------------------

RecordDeclFormatter::RecordDeclFormatter(RecordDecl* rd, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  recordDecl = rd;
  isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(recordDecl->getSourceRange().getBegin());
  sloc = rewriter.getSourceMgr().getPresumedLoc(recordDecl->getSourceRange().getBegin());
};

bool RecordDeclFormatter::isStruct() {
  return structOrUnion == STRUCT;
};

bool RecordDeclFormatter::isUnion() {
  return structOrUnion == UNION;
};

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
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: invalid struct field name " << identifier;
          errs() << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;
      }

      fieldsInFortran += "    " + tf.getFortranTypeASString(true) + " :: " + identifier + "\n";
    }
  }
  return fieldsInFortran;
}

// The procedure for any of the four sorts of structs/typedefs is fairly
// similar, but anonymous structs need to be handled specially. This 
// function puts together the name of the struct as well as the fields fetched
// from the getFortranFields() function above. All illegal names are prepended
// with h2m.
string RecordDeclFormatter::getFortranStructASString() {
  // initalize mode here
  setMode();

  string rd_buffer;
  if (!isInSystemHeader) {
    string fieldsInFortran = getFortranFields();
    if (fieldsInFortran.empty()) {
      rd_buffer = "! struct without fields may cause warning\n";
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Warning: struct without fields may cause warnings: \n";
        LineError(sloc);
      }
    }

    if (mode == ID_ONLY) {
      string identifier = recordDecl->getNameAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
      
    } else if (mode == TAG_ONLY) {
      string identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ID_TAG) {
      string identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";    
    } else if (mode == TYPEDEF) {
      string identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: invalid typedef name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ANONYMOUS) {
      string identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // Erase past all the spaces so that only the name remains (ie get rid of the "struct" part)
      size_t found = identifier.find_first_of(" ");
      while (found!=string::npos) {
        identifier.erase(0, found);
        found=identifier.find_first_of(" ",found+1);
      }
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      rd_buffer += "! ANONYMOUS struct may or may not have a declared name\n";
      string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
      // comment out temp_buf
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        rd_buffer += "! " + line + "\n";
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line " << line << " commented out \n";
          LineError(sloc);
        }
      }
    }
  }
  return rd_buffer;    
};

// Determines what sort of struct we are dealing with. I'm still
// not entirely sure what the differences is because the only kind
// I've ever run into is an ID_ONLY struct. I don't know what tag_name is.
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
  isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(funcDecl->getSourceRange().getBegin());
  sloc = rewriter.getSourceMgr().getPresumedLoc(funcDecl->getSourceRange().getBegin());
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
    
    CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(), sloc, args);
    // in some cases parameter doesn't have a name
    paramsDecl += "    " + tf.getFortranTypeASString(true) + ", value" + " :: " + pname + "\n"; // need to handle the attribute later
    index ++;
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
        if (args.getSilent() == false && args.getQuiet() == false) {
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
        if (args.getSilent() == false && args.getQuiet() == false) {
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
  string fortanFunctDecl;
  if (!isInSystemHeader and argLocValid()) {
    string funcType;
    string paramsString = getParamsTypesASString();
    string imports;
    if (!paramsString.empty()) {
      imports = "    USE iso_c_binding, only: " + getParamsTypesASString() + "\n";
    } else {
      imports = "    USE iso_c_binding\n";
    }
    imports +="    import\n";
    
    // check if the return type is void or not
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
      if (args.getQuiet() == false && args.getSilent() == false) {
        errs() << "Warning: invalid function name " << oldfuncname << " renamed " << funcname << "\n";
        LineError(sloc);
      }
    }
    fortanFunctDecl = funcType + " " + funcname + "(" + getParamsNamesASString() + ")" + " bind (C)\n";
    
    fortanFunctDecl += imports;
    fortanFunctDecl += getParamsDeclASString();
    // preserve the function body as comment
    if (funcDecl->hasBody()) {
      Stmt *stmt = funcDecl->getBody();
      clang::SourceManager &sm = rewriter.getSourceMgr();
      // comment out the entire function {!body...}
      string bodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(stmt->getSourceRange()), sm, LangOptions(), 0);
      string commentedBody;
      std::istringstream in(bodyText);
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line " << line << " commented out \n";
          LineError(sloc);
        }
        commentedBody += "! " + line + "\n";
      }
      fortanFunctDecl += commentedBody;

    }
    if (returnQType.getTypePtr()->isVoidType()) {
      fortanFunctDecl += "END SUBROUTINE " + funcname + "\n\n";   
    } else {
      fortanFunctDecl += "END FUNCTION " + funcname + "\n\n";
    }
    
  }

  return fortanFunctDecl;
};

// -----------initializer MacroFormatter--------------------
// The preprocessor is used to find the macro names in the source files. The macro is 
// more difficult to process. Both its name and definition are fetched using the lexer.
MacroFormatter::MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci,
     Arguments &arg) : md(md), args(arg) { //, ci(ci) {
    const MacroInfo *mi = md->getMacroInfo();
    SourceManager& SM = ci.getSourceManager();

    // define macro properties
    isObjectOrFunction = mi->isObjectLike();
    isInSystemHeader = SM.isInSystemHeader(mi->getDefinitionLoc());
   
    sloc = SM.getPresumedLoc(mi->getDefinitionLoc());

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
  if (!isInSystemHeader) {
    // remove all tabs
    macroVal.erase(std::remove(macroVal.begin(), macroVal.end(), '\t'), macroVal.end());

    // handle object first
    if (isObjectLike()) {
      // analyze type
      if (!macroVal.empty()) {
        if (CToFTypeFormatter::isString(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getQuiet() == false && args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed " << "h2m" << macroName << "\n";
              LineError(sloc);
            }
            fortranMacro += "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: h2m "+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: " + macroName + " = " + macroVal + "\n";
          }
        
        } else if (CToFTypeFormatter::isChar(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getQuiet() == false && args.getSilent() == false) {
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
            if (args.getQuiet() == false && args.getSilent() == false) {
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
            if (args.getQuiet() == false && args.getSilent() == false) {
              errs() << "Warning: macro with value including FUL detected. ";
              errs() << macroName << " Is invalid.\n";
              LineError(sloc);
            }
            fortranMacro = "!REAL(C_DOUBLE), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          } else if (macroName.front() == '_') {
             if (args.getQuiet() == false && args.getSilent() == false) {
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
              if (args.getSilent() == false && args.getQuiet() == false) {
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
              if (args.getSilent() == false && args.getQuiet() == false) { 
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
    FunctionDeclFormatter fdf(cast<FunctionDecl> (d), TheRewriter, args);
    allFunctionDecls += fdf.getFortranFunctDeclASString();
    
    // args.getOutput().os() << "INTERFACE\n" 
    // << fdf.getFortranFunctDeclASString()
    // << "END INTERFACE\n";      
  } else if (isa<TypedefDecl> (d)) {
    TypedefDecl *tdd = cast<TypedefDecl> (d);
    TypedefDeclFormater tdf(tdd, TheRewriter, args);
    args.getOutput().os() << tdf.getFortranTypedefDeclASString();

  } else if (isa<RecordDecl> (d)) {
    RecordDecl *rd = cast<RecordDecl> (d);
    RecordDeclFormatter rdf(rd, TheRewriter, args);
    args.getOutput().os() << rdf.getFortranStructASString();


  } else if (isa<VarDecl> (d)) {
    VarDecl *varDecl = cast<VarDecl> (d);
    VarDeclFormatter vdf(varDecl, TheRewriter, args);
    args.getOutput().os() << vdf.getFortranVarDeclASString();

  } else if (isa<EnumDecl> (d)) {
    EnumDeclFormatter edf(cast<EnumDecl> (d), TheRewriter, args);
    args.getOutput().os() << edf.getFortranEnumASString();
  } else {

    args.getOutput().os() << "!found other type of declaration \n";
    d->dump();
    RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d);
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
    cl::ParseCommandLineOptions(argc, argv, "h2m\n");
    std::unique_ptr<CompilationDatabase> Compilations;
    SmallString<256> PathBuf;
    sys::fs::current_path(PathBuf);
    Compilations.reset(new FixedCompilationDatabase(Twine(PathBuf), other));

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
      errs() << "Error opening args.getOutput() file: " << filename << error.message() << "\n";
      return(1);  // We can't possibly keep going if the file can't be opened.
    }
    if (Optimistic == true) {  // Keep all output inspite of errors
      OutputFile.keep();
    }
    Arguments args(Quiet, Silent, OutputFile, NoHeaders);  // Create an object to pass around arguments


    // Create a new clang tool to be used to run the frontend actions
    ClangTool Tool(*Compilations, SourcePaths);
    int tool_errors = 0;  // No errors have occurred running the tool yet

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
          errs() << "Alternately, enable optimistic mode (-optimist or -k) to continue despite errors.\n";
          return(initerrs);
        } else if (stackfiles.empty() == true) {  // Whatever happend is not recoverable.
          errs() << "Unrecoverable initialization error. No files recorded to translate.\n";
          return(initerrs);
        } else {
          errs() << "Optimistic run continuing.\n";
        }
      }

      // Dig through the created stack of header files we have seen, as prepared by
      // the first Clang tool which tracks the preprocessor.
      string modules_list;
      while (stackfiles.empty() == false) {
        string headerfile = stackfiles.top();
        stackfiles.pop(); 
        ClangTool stacktool(*Compilations, headerfile);  // Create a tool to run on each file in turn
        TNAFrontendActionFactory factory(modules_list, args);
        // modules_list is the growing string of previously translated modules this module may depend on
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
    }  // End processing for the -r option

    // Note that the output has already been kept if this is an optimistic run. It doesn't hurt.
    if (!tool_errors) {  // If the last run of the tool was not successful, the output may be garbage
      OutputFile.keep();
    }

    // A compiler post-process has been specified and there is an actual output file,
    // prepare and run the compiler.
    if (Compiler.size() && filename.compare("-") != 0) {
      if (system(NULL) == true) {  // A command interpreter is available
        string command = Compiler + " " + filename;
        int success = system(command.c_str());
      } else {  // Cannot run using system (fork might succeed...)
        errs() << "Error: No command interpreter available to run system process " << Compiler << "\n";
      }
    }
    return(tool_errors);
  }

  errs() << "At least one argument (header to process) must be provided.\n";
  errs() << "Run 'h2m -help' for usage details.\n";
  return(1);  // Someone did not give any arguments
};
