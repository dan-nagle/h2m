// Header file for the h2m Autofortran tool, written by
// Sisi Liu at NCAR, envisioned by Dan Nagle, and revised
// by Michelle Anderson. 
// This header includes all the necessary C/C++ headers and
// all the necessary Clang/LLVM headers to run h2m.

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

// recursive converter
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"

// lexer and writer
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"

// preprocesser
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ToolOutputFile.h"


#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <sstream>
// Set, deque, and stack are needed to keep track of files seen by the preprocessor
#include <set>
#include <deque>
#include <stack>
// Map is used to assign unique module names if there are duplicate file names
#include <map>
// Used to determine whether or not a character has a lowercase equivalent
#include <locale>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;


//------------Utility Classes for Argument parsing etc-------------------------------------------------------------------------------
// Used to pass arguments to the tool factories and actions so I don't have to keep changing them if more are added
// This keeps track of the quiet and silent options, as well as the output file, and allows greater flexibility
// in the future.
class Arguments {
public:
  Arguments(bool q, bool s, llvm::tool_output_file &out, bool sysheaders,
      bool t, bool a, bool b, bool h) :
      quiet(q), silent(s), output(out), no_system_headers(sysheaders) ,
      together(t), array_transpose(a), auto_bind(b), hide_macros(h)
      { module_name = ""; }
  llvm::tool_output_file &getOutput() { return output; }
  bool getQuiet() { return quiet; } 
  bool getSilent() { return silent; }
  bool getNoSystemHeaders() { return no_system_headers; }
  string getModuleName() { return module_name; }
  void setModuleName(string newstr) { module_name = newstr; }
  bool getTogether() { return together; }
  bool getArrayTranspose() { return array_transpose; }
  bool getAutobind() { return auto_bind; }
  bool getHideMacros() { return hide_macros; }
  string GenerateModuleName(string Filename);
  
private:
  // Where to send translated Fortran code
  llvm::tool_output_file &output;
  // Should we report lines which are commented out?
  bool quiet;
  // Should we report illegal identifiers and more serious issues?
  bool silent;
  // Should we recursively translate system header files?
  bool no_system_headers;
  // Should all non-system header info. be sent to a single module?
  bool together;
  // Whether or not to transpose dimensions of encountered arrays of
  // multiple dimensions.
  bool array_transpose;
  // Should we automatically handle illegal names with BIND(C, name=...)
  bool auto_bind;
  // Whether we should comment out all function like macro definitions or
  // make approximate tranlsations.
  bool hide_macros;
  // The module name may be altered during processing by the action;
  // by default this is an empty string. It is used to pass values out, not in.
  string module_name;
};


//------------Formatter class decl----------------------------------------------------------------------------------------------------
// This class holds a variety of functions used to transform C syntax into Fortran.
class CToFTypeFormatter {
public:
  // QualTypes contain modifiers like "static" or "volatile"
  QualType c_qualType;
  // ASTContext contains detailed information not held in the AST node
  ASTContext &ac;
  // Presumed location of the record being processed.
  PresumedLoc sloc;

  CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc sloc, Arguments &arg);
  string getFortranTypeASString(bool typeWrapper);
  string getFortranIdASString(string raw_id);
  // Gets the raw dimensions for the array in either regular or reversed order as
  // requested. The form returned is "dim1, dim2, dim3"
  string getFortranArrayDimsASString();
  // The format of the arguments in a function prototype are completely different
  // from the format of any other array reference. This function creates argument format.
  string getFortranArrayArgASString(string dummy_name);
  bool isSameType(QualType qt2);
  bool isArrayType();  // Used to classify function arguments (decide if we need DIMENSION)
  static bool isIntLike(const string input);
  static bool isDoubleLike(const string input);
  static bool isType(const string input);
  static bool isString(const string input);
  static bool isChar(const string input);
  static string createFortranType(const string macroName, const string macroVal, PresumedLoc loc, Arguments &args);
  // Prints an error location.
  static void LineError(PresumedLoc sloc); 
  // Can find when a name or long is illegally long for Fortran.
  static string CheckLength(string tochek, int limit, bool no_warn, PresumedLoc sloc);
  // Constants to be used for length checking when comparing names/lines
  // to see if they are valid Fortran.
  static const int name_max = 63;
  static const int line_max = 132;
private:
  Arguments &args;
};

// This class is used to translate structs, unions, and typedefs
// into Fortran equivalents. 
class RecordDeclFormatter {
public:
  // An anonymous struct may not have a declared name.
  const int ANONYMOUS = 0;
  const int ID_ONLY = 1;
  const int TAG_ONLY = 2;
  const int ID_TAG = 3;
  const int TYPEDEF = 4;
  
  const int UNION = 0;
  const int STRUCT = 1;

  // Pointer to the AST node being inspected.
  RecordDecl *recordDecl;
  // By default, the record is an anonymous struct
  int mode = ANONYMOUS;
  bool structOrUnion = STRUCT;
  string tag_name;
  // This doesn't actually seem to be used
  bool isInSystemHeader;
  // Presumed location of this node
  PresumedLoc sloc;

  // Member functions declarations
  RecordDeclFormatter(RecordDecl *rd, Rewriter &r, Arguments &arg);
  void setMode();
  void setTagName(string name);
  bool isStruct();
  bool isUnion();
  string getFortranStructASString();
  string getFortranFields();

  // This function exists to make sure that a type is not
  // declared twice. This frequently happens with typedefs
  // renaming structs. If the identifier provided already
  // exists, "false" is returned. Otherwise "true" is returned.
  static bool StructAndTypedefGuard(string name);

private:
  // Rewriters are used, typically, to make small changes to the
  // source code. This one, however, serves a different,
  // mysterious purpose
  Rewriter &rewriter;
  // Arguments passed in from the action factory. This includes
  // quiet/silent, the module's name etc
  Arguments &args;
  
};

// This class is used to translate a C enumeration into a
// Fortran equivalent.
class EnumDeclFormatter {
public:
  EnumDecl *enumDecl;
  // This doesn't appear to be used.
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg);
  // The main function to fetch the enumeration as a Fortran
  // string equivalent.
  string getFortranEnumASString();

private:
  Rewriter &rewriter;
  // Arguments passed in from the action factory.
  Arguments &args;
  
};

// This class is used to translate a variable declaration into
// a Fortran equivalent.
class VarDeclFormatter {
public:
  VarDecl *varDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg);
  // Get the initalization value of the variable.
  string getInitValueASString();
  // Get the declaration of the variable.
  string getFortranVarDeclASString();
  // Arrays are special and complicated. They must be
  // handled seperately.
  string getFortranArrayDeclASString();
  // Attempt to get the initialization of a structured type
  // in fortran. This is also quite complex and specialized.
  // The name of the structure is passed in to allow the possibility of
  // recursion.
  string getFortranStructDeclASString(string struct_name);
  void getFortranArrayEleASString(InitListExpr *ile, string &arrayValues, string arrayShapes,
      bool &evaluatable, bool firstEle, bool is_char);

private:
  Rewriter &rewriter;
  // Used to store information about the shape of an array declaration.
  string arrayShapes_fin;
  Arguments &args;
  
};

// This class translates C typedefs into 
// the closest possible fortran equivalent.
class TypedefDeclFormater {
public:
  TypedefDecl *typedefDecl;
  bool isInSystemHeader;
  // The presumed location of the node
  PresumedLoc sloc;

  // Member functions declarations
  TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &args);
  string getFortranTypedefDeclASString();

private:
  Rewriter &rewriter;
  // Whether or not the location of the node is valid according to clang
  bool isLocValid;
  // Arguments passed in from the action factory
  Arguments &args;
  
};

// Class to translate a C function declaration into either a Fortran
// function or subroutine as appropriate.
class FunctionDeclFormatter {
public:
  FunctionDecl *funcDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, Arguments &arg);
  string getParamsNamesASString();
  string getParamsDeclASString();
  // Fetches the entire declaration using the other helpers seen here
  string getFortranFunctDeclASString();
  string getParamsTypesASString();
  // Whether or not the argument locations are valid according to clang
  bool argLocValid();

private:
  QualType returnQType;
  // The parameters of the function are kept in an array
  llvm::ArrayRef<ParmVarDecl *> params;
  Rewriter &rewriter;
  // Arguments passed in from the action factory
  Arguments &args;
};


// Class used to translate Macros into the closest appropriate 
// Fortran equivalent. This may be a constant value or a subroutine
// or function. Some macros cannot be translated and are commented
// out instead.
class MacroFormatter {
public:
  const MacroDirective *md;
  string macroName;
  string macroVal;
  string macroDef;
  bool isInSystemHeader;
  // Presumed location of the macro's start according to clang
  PresumedLoc sloc;

  MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci, Arguments &arg);
  bool isObjectLike();
  bool isFunctionLike();
  string getFortranMacroASString();


private:
  bool isObjectOrFunction;
  // Arguments passed in from the action factory
  Arguments &args;
  CompilerInstance &ci;
};



