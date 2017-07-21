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
// and for a number of other uses.
#include <set>
#include <deque>
#include <stack>
// Map is used to assign unique module names if there are duplicate file names
#include <map>
// Used to determine whether or not a character has a lowercase equivalent
#include <locale>

// These were here when I got here (though it may not be a good idea) 
// and it is too difficult to take them out now...
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
      bool t, bool a, bool b, bool h, bool u) :
      quiet(q), silent(s), output(out), no_system_headers(sysheaders) , together(t),
      array_transpose(a), auto_bind(b), hide_macros(h), detect_invalid(u)
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
  bool getDetectInvalid() { return detect_invalid; }
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
  // or just prepend h2m and have the user fix them?
  bool auto_bind;
  // Whether we should comment out all function like macro definitions or
  // make approximate tranlsations.
  bool hide_macros;
  // Whether to comment out invalid types which are detected
  bool detect_invalid;
  // The module name may be altered during processing by the action;
  // by default this is an empty string. It is used to pass values out, not in.
  string module_name;
};


//------------Formatter class decl----------------------------------------------------------------------------------------------------
// This class holds a variety of functions used to transform C syntax into Fortran.
class CToFTypeFormatter {
public:
  // Public status codes to be referenced by the formatter objects.
  // FUNC_MACRO, is not an error code, just a statement that we are
  // processing a function macro. U_OR_L_MACRO is for an object like
  // macro with some unrecognized size/type modifier (ie l/L or u/U).
  // BAD_STRUCT_TRANS is for failed translaiton of a structure init.
  // DUPLICATE is for duplicate identifiers of any kind. BAD_TYPE
  // is for an anonymous, unrecognized or va_list related type in
  // a declaration. BAD_ANON is for the declaration of an anonymous
  // structure type. BAD_ARRAY is for a failed array initialization
  // evaluation. BAD_MACRO is for a confusing macro of any kind.
  // CRIT_ERROR is for nullpointers and other serious, internal problems.
  // BAD_STAR_ARRAY is for a variable size array outside of a 
  // function prototype. UNKNOWN_VAR is for an unrecognized variable
  // intialiation declaration. Others are self explanatory.
  enum status {OKAY, FUNC_MACRO, BAD_ANON, BAD_LINE_LENGTH, BAD_TYPE,
     BAD_NAME_LENGTH, BAD_STRUCT_TRANS, BAD_STAR_ARRAY, DUPLICATE,
     U_OR_L_MACRO, UNKNOWN_VAR, CRIT_ERROR, BAD_MACRO, BAD_ARRAY};
  typedef enum status status;

  // QualTypes contain modifiers like "static" or "volatile"
  QualType c_qualType;
  // ASTContext contains detailed information not held in the AST node
  ASTContext &ac;
  // Presumed location of the record being processed.
  PresumedLoc sloc;

  CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc sloc, Arguments &arg);
  // Get the Fortran "TYPE(type_name)" or "type_name" phrase coresponding
  // to the current type of declaration under consideration. In the case that
  // the translation can't be completed, problem is set to FALSE. It must be
  // true initially.
  string getFortranTypeASString(bool typeWrapper, bool &problem);
  // Gets the raw string, except in an array where dimensions are needed.
  string getFortranIdASString(string raw_id);
  // Gets the raw dimensions for the array in either regular or reversed order as
  // requested. The form returned is "dim1, dim2, dim3". 
  string getFortranArrayDimsASString();
  // The format of the arguments in a function prototype are completely different
  // from the format of any other array reference. This function creates argument format.
  string getFortranArrayArgASString(string dummy_name);
  bool isSameType(QualType qt2);
  // Used to classify function arguments (decide if we need DIMENSION atributes)
  bool isArrayType();
  // Here are functions to determine whether or not a macro resembles a given type.
  static bool isIntLike(const string input);
  static bool isDoubleLike(const string input);
  static bool isType(const string input);
  static bool isString(const string input);
  static bool isChar(const string input);
  static bool isHex(const string in_str);
  static bool isBinary(const string in_str);
  static bool isOctal(const string in_str);
  static string DetermineIntegerType(const string integer_in, bool &invalid); 
  static string DetermineFloatingType(const string integer_in, bool &invalid); 
  static string GroomFloatingType(const string in);
  static string GroomIntegerType(const string in);

  // This function emits a standard error relating to the frequent need
  // to prepend "h2m" to the front of an illegal identifier.
  static void PrependError(const string identifier, Arguments& args, PresumedLoc sloc);
  // This is used to create a typedef like macro. These are approximate
  // and only work on some types (short, int, long...)
  static string createFortranType(const string macroName, const string macroVal,
      PresumedLoc loc, Arguments &args);
  // This somewhat complicated function handles emitting all errors and returning
  // a (potentially) commented out string to the main translation routine.
  static string EmitTranslationAndErrors(status current_status, string error_string,
      string translation_string, PresumedLoc sloc, Arguments &args);
  // Prints an error location.
  static void LineError(PresumedLoc sloc); 
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
  // Member functions declarations
  RecordDeclFormatter(RecordDecl *rd, Rewriter &r, Arguments &arg);
  void setMode();
  void setTagName(string name);
  bool isStruct();
  bool isUnion();
  string getFortranStructASString();
  string getFortranFields();
  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 
  PresumedLoc getSloc() { return sloc; }

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
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;
};

// This class is used to translate a C enumeration into a
// Fortran equivalent.
class EnumDeclFormatter {
public:
   // Member functions declarations
  EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg);
  // The main function to fetch the enumeration as a Fortran
  // string equivalent.
  string getFortranEnumASString();
  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 
  PresumedLoc getSloc() { return sloc; }

private:
  EnumDecl *enumDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;
  Rewriter &rewriter;
  // Arguments passed in from the action factory.
  Arguments &args;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;
};

// This class is used to translate a variable declaration into
// a Fortran equivalent.
class VarDeclFormatter {
public:
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
  // This function is set up to do recursion to fetch a structure's
  // fields when that structure is initialized
  string getFortranStructFieldsASString(Expr *exp);
  // Fetches an individual initialized array element.
  void getFortranArrayEleASString(InitListExpr *ile, string &arrayValues,
      string &arrayShapes, bool &evaluatable, bool firstEle, bool is_char);
  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 
  PresumedLoc getSloc() { return sloc; }

private:
  Rewriter &rewriter;
  // Used to store information about the shape of an array declaration.
  string arrayShapes_fin;
  // Extra Arguments passed in from the action factory
  Arguments &args;
  // The variable declaration we are actually looking at.
  VarDecl *varDecl;
  bool isInSystemHeader;
  // The presumed location of this node
  PresumedLoc sloc;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;

};

// This class translates C typedefs into 
// the closest possible fortran equivalent.
class TypedefDeclFormater {
public:
  // Member functions declarations
  TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &args);
  string getFortranTypedefDeclASString();
  PresumedLoc getSloc() { return sloc; }
  CToFTypeFormatter::status getStatus() { return current_status; }
  string getErrorString() { return error_string; }

private:
  bool isInSystemHeader;
  // The presumed location of the node
  PresumedLoc sloc;
  TypedefDecl *typedefDecl;
  Rewriter &rewriter;
  // Whether or not the location of the node is valid according to clang
  bool isLocValid;
  // Arguments passed in from the action factory
  Arguments &args;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;
};

// Class to translate a C function declaration into either a Fortran
// function or subroutine as appropriate.
class FunctionDeclFormatter {
public:

  // Member functions declarations
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, Arguments &arg);
  // These are the untyped parameters, used in the first line of the
  // function declaration.
  string getParamsNamesASString();
  // This is used to fetch the types as well as the names of the 
  // parameters to complete the interface.
  string getParamsDeclASString();
  // Fetches the entire declaration using the other helpers seen here
  string getFortranFunctDeclASString();
  // This helper fetches the types of the parameters for use in the
  // use iso_c_binding only: <types> statement
  string getParamsTypesASString();
  // Whether or not the argument locations are valid according to clang
  bool argLocValid();
  PresumedLoc getSloc() { return sloc; }
  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 

private:
  // The qualified type of the return value of the function 
  QualType returnQType;
  // The parameters of the function are kept in an array ref
  llvm::ArrayRef<ParmVarDecl *> params;
  // Again, this appears to be included from some previous incarnation
  // of the h2m software and not actually in use as a "rewriter"
  Rewriter &rewriter;
  // Arguments passed in from the action factory
  Arguments &args;
  FunctionDecl *funcDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;

};


// Class used to translate Macros into the closest appropriate 
// Fortran equivalent. This may be a constant value or a subroutine
// or function. Some macros cannot be translated and are commented
// out instead.
class MacroFormatter {
public:
  MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci, Arguments &arg);
  bool isObjectLike();
  bool isFunctionLike();
  string getFortranMacroASString();

  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 
  PresumedLoc getSloc() { return sloc; }
private:
  const MacroDirective *md;
  // Name of the macro
  string macroName;
  // Value of the macro if it has something we can call a "value"
  string macroVal;
  // The string definition of the macro
  string macroDef;
  bool isInSystemHeader;
  // Presumed location of the macro's start according to clang
  PresumedLoc sloc;

  // Whether this is like an object (ie like a Char, Char*, int, double...)
  // or like a function. Function likes are translated to subroutines but
  // it's really not a very effective translation.
  bool isObjectOrFunction;
  // Arguments passed in from the action factory
  Arguments &args;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;
  CompilerInstance &ci;
};



