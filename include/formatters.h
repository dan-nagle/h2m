// Header file for the h2m-AutoFortran tool, written by
// Sisi Liu at NCAR, envisioned by Dan Nagle, and revised
// by Michelle Anderson. 
// This header includes all the necessary C/C++ headers and
// all the necessary Clang/LLVM headers to run h2m.
// All the helper classes used for translation of AST
// nodes and argument handling are also in this file.

// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/Version.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Path.h"
#include "llvm/Config/llvm-config.h"

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

#if LLVM_VERSION_MAJOR < 6
#define ToolOutputFile tool_output_file
#endif

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


//------------Utility Classes for Argument parsing etc--------------------------
class Arguments;  // This class uses part of CToFTypeFormatter, but CTFTF needs it, too

//------------Formatter class decl----------------------------------------------
// This class holds a variety of functions used to transform C syntax into Fortran.
class CToFTypeFormatter {
public:
  // Public status codes to be referenced by the formatter objects.
  // FUNC_MACRO, is not an error code, just a statement that we are
  // processing a function macro. U_OR_L_MACRO is for an object like
  // macro with some unrecognized size/type modifier (now obsolete)
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
  enum status {OKAY=0, FUNC_MACRO=1, BAD_ANON=2, BAD_LINE_LENGTH=3, BAD_TYPE=4,
     BAD_NAME_LENGTH=5, BAD_STRUCT_TRANS=6, BAD_STAR_ARRAY=7, DUPLICATE=8,
     U_OR_L_MACRO=9, UNKNOWN_VAR=10, CRIT_ERROR=11, BAD_MACRO=12, BAD_ARRAY=13};
  typedef enum status status;

  // QualTypes contain modifiers like "static" or "volatile"
  // and information about an AST node's type
  QualType c_qualType;
  // ASTContext contains detailed information not held in the AST node
  ASTContext &ac;
  // Presumed location of the record being processed (file location).
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
  // requested in this.Arguments. The form returned is "dim1, dim2, dim3". 
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
  // Does this macro define a new type? This kind of macro is quite rare.
  static bool isType(const string input);
  // These functions help determine a macro's type and proper translation.
  static bool isString(const string input);
  static bool isChar(const string input);
  static bool isHex(const string in_str);
  static bool isBinary(const string in_str);
  static bool isOctal(const string in_str);
  // This determines the fortran KIND for an integer-like macro.
  static string DetermineIntegerType(const string integer_in, bool &invalid); 
  // This determines the fortran KIND for a float like macro.
  static string DetermineFloatingType(const string integer_in, bool &invalid); 
  // These static functions remove questionable characters
  // from floats, integers, and hex types respectively to 
  // help create macros.
  static string GroomFloatingType(const string in);
  static string GroomIntegerType(const string in);
  static string GroomHexType(const string in);

  // This function emits a standard error relating to the frequent need
  // to prepend "h2m" to the front of an illegal identifier.
  static void PrependError(const string identifier, Arguments& args, PresumedLoc sloc);
  // This is used to create a typedef like macro. These are approximate
  // and only work on some types (short, int, long...)
  static string createFortranType(const string macroName, const string macroVal,
      PresumedLoc loc, Arguments &args);
  // This somewhat complicated function handles emitting all errors and returning
  // a (potentially) commented out string to the main translation routine as
  // needed according to the status passed in and the values in args.
  static string EmitTranslationAndErrors(status current_status, string error_string,
      string translation_string, PresumedLoc sloc, Arguments &args);
  // Prints an error location (file and line).
  static void LineError(PresumedLoc sloc); 
  // Constants to be used for length checking when comparing names/lines
  // to see if they are valid Fortran.
  static const int name_max = 63;
  static const int line_max = 132;
private:
  Arguments &args;
};

//------------Utility Classes for Argument parsing etc--------------------------
// This is used to pass arguments to the tool factories and actions so I don't have to keep
// changing them if more are added. This keeps track of the quiet and silent options,
// as well as the output file, and allows greater flexibility in the future.
class Arguments {
public:
  // Initialize all these boolean values to determine what is commented
  // out/warned about. q is whether the run is quiet. s is whether it 
  // is silent. sysheaders is whether to exclude system header form a 
  // recursive line. t is whether to put all includes in one module. a
  // is whether to transpose array dimensions. b is whether to bind to
  // illegal C names automatically. h is whether to hide function like
  // macros (don't translate them) The remaining options are whether
  // or not to NOT comment out things h2m normally checks for.
  Arguments(bool q, bool s, llvm::ToolOutputFile &out, bool sysheaders,
      bool t, bool a, bool b, bool h, bool bad_name_length,
      bool bad_line_length, bool bad_type, bool bad_anon, bool duplicate) :
      quiet(q), silent(s), output(out), no_system_headers(sysheaders) ,
      together(t), array_transpose(a), auto_bind(b), hide_macros(h) {
     module_name = "";
     int i = 0;
     // Initialize the array which tells us what problems, 
     // usually commented out, should be ignored. The defaults
     // are to ignore nothing.
     for (i = 0; i < CToFTypeFormatter::BAD_ARRAY; i++) {
       should_ignore[i] = false;
     }
     // Now initialize the specifically passed in options.
     // Take advantage of the enumerated type to index the array.
     should_ignore[CToFTypeFormatter::BAD_NAME_LENGTH] = bad_name_length;
     should_ignore[CToFTypeFormatter::BAD_LINE_LENGTH] = bad_line_length;
     should_ignore[CToFTypeFormatter::BAD_TYPE] = bad_type;
     should_ignore[CToFTypeFormatter::BAD_ANON] = bad_anon;
     should_ignore[CToFTypeFormatter::DUPLICATE] = duplicate; 

   }
  // These functions are setters and getters for the arguments members.
  llvm::ToolOutputFile &getOutput() { return output; }
  bool getQuiet() { return quiet; } 
  bool getSilent() { return silent; }
  bool getNoSystemHeaders() { return no_system_headers; }
  string getModuleName() { return module_name; }
  // This function (only to be called once for each file) will
  // generate a unique module name based on a file name.
  void setModuleName(string newstr) { module_name = newstr; }
  bool getTogether() { return together; }
  bool getArrayTranspose() { return array_transpose; }
  bool getAutobind() { return auto_bind; }
  bool getHideMacros() { return hide_macros; }
  // This will tell us if we should comment out problems 
  // associated witht he status passed in as status_num.
  // A value of true means we should comment the status
  // out. A value of false means we should not.
  bool ShouldCommentOut(CToFTypeFormatter::status status_num) {
    // These are stored with true indicating we shouldn't
    // comment out a certain kind of problem.
    return !should_ignore[status_num];
  }
  string GenerateModuleName(string Filename);
  
private:
  // Where to send translated Fortran code
  llvm::ToolOutputFile &output;
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
  // This array holds options to NOT comment out various problems for
  // which h2m ususally checks. BAD_ARRAY is the last member of the 
  // enumerated type. Add an extra 1 to account for zero indexing.
  // should_ignore[BAD_ARRAY], for example will contain true
  // if we should not comment out that bad array problems, and false
  // otherwise.
  bool should_ignore[CToFTypeFormatter::BAD_ARRAY + 1];
  // The module name may be altered during processing by the action;
  // by default this is an empty string. It is used to pass values out, not in.
  string module_name;
};


// This class is used to translate structs, unions, and typedefs
// into Fortran equivalents. 
class RecordDeclFormatter {
public:
  // Member functions declarations
  RecordDeclFormatter(RecordDecl *rd, Rewriter &r, Arguments &arg);
  // These get information about how to get the proper identifier
  // for the struct or union
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
  // It is in this class because originally it was only used
  // on structures and typedefs (the most common offenders).
  // This reasoning is now historical.
  static bool StructAndTypedefGuard(string name);

private:
  // Rewriters are used, typically, to make small changes to the
  // source code. This one, however, is left over from a previous
  // code incarnation, but still used for things like geting the
  // source manager or compiler instance.
  Rewriter &rewriter;
  // Arguments passed in from the action factory. This includes
  // quiet/silent, the module's name etc
  Arguments &args;
  // An anonymous struct may not have a declared name.
  // These should be in a typedef but they are not -Michelle
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
  // This is used to keep system headers out of the translation
  // because we do not want them to leak in.
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
// Fortran equivalent. Note that Fortran enums don't actually
// have names.
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
  // Again, this keeps system header definitions from leaking
  // into the local translation.
  bool isInSystemHeader;
  // Presumed location within the source files.
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
// a Fortran equivalent. It handles structs and arrays and will
// attempt to translate an initialization as well.
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
  // The presumed location of this node in the source files.
  PresumedLoc sloc;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success 
  // or failure (as the case may be).
  CToFTypeFormatter::status current_status;
};

// This class translates C typedefs into 
// the closest possible fortran equivalent, a TYPE
// with a single member. These are not interoperable.
class TypedefDeclFormater {
public:
  // Member functions declarations
  TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &args);
  string getFortranTypedefDeclASString();
  PresumedLoc getSloc() { return sloc; }
  CToFTypeFormatter::status getStatus() { return current_status; }
  string getErrorString() { return error_string; }

private:
  // This is used to keep out definitions from system headers.
  bool isInSystemHeader;
  // The presumed location of the node in the source files.
  PresumedLoc sloc;
  TypedefDecl *typedefDecl;
  // This is now only used to fetch information, not to rewrite anything.
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

// This class translates a C function declaration into either a Fortran
// function or subroutine as appropriate.
class FunctionDeclFormatter {
public:
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, Arguments &arg);
  // This gives the untyped parameters, used in the first line of the
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
  // The qualified type of the return value of the function. It
  // may be void in which case we create a corresponding subroutine.
  QualType returnQType;
  // The parameters of the function are kept in an array ref during
  // processing.
  llvm::ArrayRef<ParmVarDecl *> params;
  // Again, this appears to be included from some previous incarnation
  // of the h2m software and not actually in use as a "rewriter"
  Rewriter &rewriter;
  // Arguments passed in from the action factory
  Arguments &args;
  FunctionDecl *funcDecl;
  // This is used to keep system header definitions out of the
  // local translation.
  bool isInSystemHeader;
  // The location of the declaration in the source files.
  PresumedLoc sloc;
  // The initially empty string which describes the text where
  // an error occurred, if it did occur.
  string error_string;
  // The status code which represents the translation's success.
  CToFTypeFormatter::status current_status;

};


// This class is used to translate Macros into the closest appropriate 
// Fortran equivalent. This may be a constant value or a subroutine
// or function. Some macros cannot be translated and are commented
// out instead. It is not fool-proof. It can be fooled into giving
// an incorrect translation, but someone would have to be trying
// to write h2m-confusing code.
class MacroFormatter {
public:
  MacroFormatter(const Token MacroNameTok, const MacroDirective *md, 
      CompilerInstance &ci, Arguments &arg);
  // Is this macro like a string, int, double, char, or type definition?
  bool isObjectLike();
  bool isFunctionLike();
  string getFortranMacroASString();

  string getErrorString() { return error_string; }
  CToFTypeFormatter::status getStatus() { return current_status; } 
  PresumedLoc getSloc() { return sloc; }
private:
  const MacroDirective *md;
  string macroName;
  // Value of the macro if it has something we can call a "value"
  string macroVal;
  // The string definition of the macro
  string macroDef;
  // This is used to keep system headers out of the local translation.
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



