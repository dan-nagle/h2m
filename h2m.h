// Header file for the h2m Autofortran tool, written by
// Sisi Liu at NCAR, envisioned by Dan Nagle, and revised
// by Michelle Anderson. 

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
  Arguments(bool q, bool s, llvm::tool_output_file &out, bool sysheaders, bool t) :
     quiet(q), silent(s), output(out), no_system_headers(sysheaders) ,
     together(t) { module_name = ""; }
  llvm::tool_output_file &getOutput() { return output; }
  bool getQuiet() { return quiet; } 
  bool getSilent() { return silent; }
  bool getNoSystemHeaders() { return no_system_headers; }
  string getModuleName() { return module_name; }
  void setModuleName(string newstr) { module_name = newstr; }
  bool getTogether() { return together; }
  
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
  bool isSameType(QualType qt2);
  static bool isIntLike(const string input);
  static bool isDoubleLike(const string input);
  static bool isType(const string input);
  static bool isString(const string input);
  static bool isChar(const string input);
  static string createFortranType(const string macroName, const string macroVal, PresumedLoc loc, Arguments &args);
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
  void getFortranArrayEleASString(InitListExpr *ile, string &arrayValues, string arrayShapes, bool &evaluatable, bool firstEle);

private:
  Rewriter &rewriter;
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


//------------Visitor class decl----------------------------------------------------------------------------------------------------

// Main class which works to translate the C to Fortran by calling helpers.
// It performs actions to translate every node in the AST. It keeps track
// of all seen functions so that it can put them together after the vars, macros 
// and structures are translated.
class TraverseNodeVisitor : public RecursiveASTVisitor<TraverseNodeVisitor> {
public:
  TraverseNodeVisitor(Rewriter &R, Arguments& arg) :
	  TheRewriter(R), args(arg) {}

  // Traverse all declaration nodes. Note that Clang AST nodes do NOT all have
  // a common ancestor. Decl and Stmt are essentially unrelated.
  bool TraverseDecl(Decl *d);
  bool TraverseStmt(Stmt *x);
  bool TraverseType(QualType x);
  // All the function declarations processed so far in this AST.
  string allFunctionDecls;

private:
  Rewriter &TheRewriter;
  // Additional translation arguments (ie quiet/silent) from the action factory
  Arguments &args;
};

// Traces the preprocessor as it moves through files and records the inclusions in a stack
// using a set to keep track of files already seen. This allows them to be translated in
// the reverse order of inclusions so dependencies can be maintained. This is used for the
// recursive -r option of h2m.
class TraceFiles : public PPCallbacks {
public:
  TraceFiles(CompilerInstance &ci, std::set<string>& filesseen, std::stack<string>& filesstack, Arguments& arg) :
  ci(ci), seenfiles(filesseen), stackfiles(filesstack), args(arg) { }

  // Writes into the stack if a new file is entered by the preprocessor and the file does
  // not yet exist in the set, thus creating an exclusive reverse-ordered stack.
  void FileChanged(clang::SourceLocation loc, clang::PPCallbacks::FileChangeReason reason,
        clang::SrcMgr::CharacteristicKind filetype, clang::FileID prevfid) override {

    // We have found a system header and have been instructured to skip it, so we move along
    if (loc.isValid() == false) {
      return;  // We are not in a valid file. Don't include it. It's probably an error.    
    } else if (ci.getSourceManager().isInSystemHeader(loc) == true && args.getNoSystemHeaders() == true) {
      return;
    }
    // This is already guarded by the loc.isValid() above so we know that loc is valid when we ask this
    clang::PresumedLoc ploc = ci.getSourceManager().getPresumedLoc(loc);
    string filename = ploc.getFilename();
    if (seenfiles.find(filename) != seenfiles.end()) {
      // Place holder: we have seen the file before so we don't add it to the stack.
      return;
     } else if (filename.find("<built-in>") != string::npos || filename.find("<command line>") != string::npos) {
       // These are not real files. They may be called something else on other platforms, but
       // this was the best way I could think to try to get rid of them. They should not be
       // translated in a recursive run. They do not actually exist. This doesn't actually fix the problem.
       return;
     } else {
      // New file. Add it to the stack and the set.
      seenfiles.insert(filename);
      stackfiles.push(filename);
    }
  }

private:
  CompilerInstance &ci;
  // Recording data structures to keep track of files the preprocessor sees
  std::set<string>& seenfiles;
  // Order data structure to keep track of the order the files were seen in
  std::stack<string>& stackfiles;
  Arguments &args;
};

// This is a dummy class. See the explanation for the existence of
// CreateHeaderStackAction::CreateASTConsumer.
class InactiveNodeConsumer : public clang::ASTConsumer {
public:
  InactiveNodeConsumer() {}

  // Intentionally does nothing. We're only using the preprocessor.
  virtual void HandleTranslationUnit(clang::ASTContext &Context) {}
};

// Action to follow the preprocessor and create a stack of files to be dealt with
// and translated into fortran in the order seen so as to have the proper order
// of USE statements in recursive processing.
class CreateHeaderStackAction : public clang::ASTFrontendAction {
public:
  CreateHeaderStackAction(std::set<string>& filesseen, std::stack<string>& filesstack, Arguments &arg) :
     seenfiles(filesseen), stackfiles(filesstack), args(arg) {} 

  // When a source file begins, the callback to trace filechanges is registered
  // so that all changes are recorded and the order of includes can be preserved
  // in the stack.
  bool BeginSourceFileAction(CompilerInstance &ci, StringRef Filename) override {
    Preprocessor &pp = ci.getPreprocessor();
    pp.addPPCallbacks(llvm::make_unique<TraceFiles>(ci, seenfiles, stackfiles, args));
    return true;
   }

  // This is a dummy. For some reason or other, in order to get the action to
  // work, I had to use an ASTFrontendAction (PreprocessorOnlyAction did not
  // work) thus this method has to exist (it is pure virtual).
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return llvm::make_unique<InactiveNodeConsumer>();
  }

private:
  // Keeps track of which headers we have seen
  std::set<string>& seenfiles;
  // Keeps track of the order the headers were seen in
  std::stack<string>& stackfiles; 
  // Arguments passed in from the action factory
  Arguments &args;
};

// Factory to run the preliminary preprocessor file tracing action defined above;
// determines the order to recursively translate header files with the help
// or a set and a stack
class CHSFrontendActionFactory : public FrontendActionFactory {
public:
  CHSFrontendActionFactory(std::set<string>& seenfiles, std::stack<string>& stackfiles, Arguments &arg) :
     seenfiles(seenfiles), stackfiles(stackfiles), args(arg) {} 

  // Creates a new action which only attends to file changes in the preprocessor.
  // This allows tracing of the files included.
  CreateHeaderStackAction *create() override {
    return new CreateHeaderStackAction(seenfiles, stackfiles, args);
  }

private:
  // Set to keep track of all the files we have seen
  std::set<string>& seenfiles;
  // Stack to keep track of the order for translation of files
  std::stack<string>& stackfiles;
  // Additional arguments, including quiet/silent and the module name
  Arguments &args;
};
  
// Classes, specifications, etc for the main translation program!
//-----------PP Callbacks functions----------------------------------------------------------------------------------------------------
// Class used by the main TNActions to inspect and translate C macros.
// It pays no other attention to the preprocessor. This class is 
// created by the TNActions and then follows the preprocessor throughout
// a file and finds all macros in that file.
class TraverseMacros : public PPCallbacks {
public:

  explicit TraverseMacros(CompilerInstance &ci, Arguments &arg)
  : ci(ci), args(arg) {}//, SM(ci.getSourceManager()), pp(ci.getPreprocessor()),

  // Call back to translate each macro when it is defined
  void MacroDefined (const Token &MacroNameTok, const MacroDirective *MD); 
private:
  CompilerInstance &ci;
  // Additional arguments passed in from the action factory
  Arguments &args;
};

  //-----------the main program----------------------------------------------------------------------------------------------------

// The class which begins the translation process. HandleTranslationUnit
// is the main entry into the Clang AST. TranslationUnit is the overarching
// unit found in each AST.
class TraverseNodeConsumer : public clang::ASTConsumer {
public:
  TraverseNodeConsumer(Rewriter &R, Arguments &arg) : Visitor(R, arg), args(arg)  {}

  // The entry function into the Clang AST as described above. From here,
  // nodes are translated recursively.
  virtual void HandleTranslationUnit(clang::ASTContext &Context);

private:
// A RecursiveASTVisitor implementation.
  TraverseNodeVisitor Visitor;
  // Additional arguments passed in from the action factory (quiet/silent)
  Arguments &args;
};

// Main translation action to be carried out on a C header file.
// This class defines all the actions to be carried out when a
// source file is processed, including what to put at the start and
// the end (the MODULE... END MODULE boilerplate).
class TraverseNodeAction : public clang::ASTFrontendAction {
public:

  TraverseNodeAction(string to_use, Arguments &arg) :
       use_modules(to_use), args(arg) {}

  // // for macros inspection
  bool BeginSourceFileAction(CompilerInstance &ci, StringRef Filename) override;

  // Action at the completion of a source file traversal, after code translation
  void EndSourceFileAction() override;

  // Returns an AST consumer which does the majority of the translation work.
  // The AST consumer keeps track of how to handle the AST nodes (what functions to call)
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
    return llvm::make_unique<TraverseNodeConsumer>(TheRewriter, args);
  }

private:
  // As previously mentioned, I don't think this serves any real purpose anymore.
  // It is used to fetch compiler instances, but not to actually rewrite source
  // code anymore. 
  Rewriter TheRewriter;
  // The full, absolute path of the file under consideration
  string fullPathFileName;
  // Modules to include in USE statements in this file's module
  string use_modules;
  // Additional arguments passed in from the action factory
  Arguments &args;
};

// Clang tools run FrontendActionFactories which implement
// a method to return a new Action for each file. However,
// only one file will be processed at a time by h2m. A new
// action factory will be created for each file. Note that
// TNA stands for "traverse node action" which is the main
// h2m action to translate C to Fortran.
class TNAFrontendActionFactory : public FrontendActionFactory {
public:
  TNAFrontendActionFactory(string to_use, Arguments &arg) :
     use_modules(to_use), args(arg) {};

  // Mandatory function to create a file's TNAction. This method
  // is called once for each file under consideration.
  TraverseNodeAction *create() override {
    return new TraverseNodeAction(use_modules, args);
  }

private:
  // Modules previously written to be included in USE statements
  // are kept here as a string which is prepended into the module.
  string use_modules;
  // Additional arguments (ie quiet/silent)
  Arguments &args;
};


