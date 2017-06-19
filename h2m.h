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
#include <map>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;


//------------Utility Classes for Argument parsing etc-------------------------------------------------------------------------------
// Used to pass arguments to the tool factories and actions so I don't have to keep changing them if more are added
// This keeps track of the queit and silent options, as well as the output file, and allows greater flexibility
// in the future.
class Arguments {
public:
  Arguments(bool q, bool s, llvm::tool_output_file &out, bool sysheaders) : quiet(q), silent(s),
      output(out), no_system_headers(sysheaders) { module_name = ""; }
  llvm::tool_output_file &getOutput() { return output; }
  bool getQuiet() { return quiet; } 
  bool getSilent() { return silent; }
  bool getNoSystemHeaders() { return no_system_headers; }
  string getModuleName() { return module_name; }
  void setModuleName(string newstr) { module_name = newstr; }
  
private:
  // Where to send translated Fortran code
  llvm::tool_output_file &output;
  // Should we report lines which are commented out?
  bool quiet;
  // Should we report illegal identifiers and more serious issues?
  bool silent;
  // Should we recursively translate system header files?
  bool no_system_headers;
  // The module name may be altered during processing by the action
  // by default this is an empty string. It is used to pass values out, not in.
  string module_name;
};


//------------Formatter class decl----------------------------------------------------------------------------------------------------
class CToFTypeFormatter {
public:
  QualType c_qualType;
  ASTContext &ac;
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

class RecordDeclFormatter {
public:
  const int ANONYMOUS = 0;
  const int ID_ONLY = 1;
  const int TAG_ONLY = 2;
  const int ID_TAG = 3;
  const int TYPEDEF = 4;
  
  const int UNION = 0;
  const int STRUCT = 1;

  RecordDecl *recordDecl;
  int mode = ANONYMOUS;
  bool structOrUnion = STRUCT;
  string tag_name;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  RecordDeclFormatter(RecordDecl *rd, Rewriter &r, Arguments &arg);
  void setMode();
  void setTagName(string name);
  bool isStruct();
  bool isUnion();
  string getFortranStructASString();
  string getFortranFields();



private:
  Rewriter &rewriter;
  Arguments &args;
  
};

class EnumDeclFormatter {
public:
  EnumDecl *enumDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg);
  string getFortranEnumASString();

private:
  Rewriter &rewriter;
  Arguments &args;
  
};

class VarDeclFormatter {
public:
  VarDecl *varDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg);
  string getInitValueASString();
  string getFortranVarDeclASString();
  string getFortranArrayDeclASString();
  void getFortranArrayEleASString(InitListExpr *ile, string &arrayValues, string arrayShapes, bool &evaluatable, bool firstEle);

private:
  Rewriter &rewriter;
  string arrayShapes_fin;
  Arguments &args;
  
};

class TypedefDeclFormater {
public:
  TypedefDecl *typedefDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &args);
  string getFortranTypedefDeclASString();

private:
  Rewriter &rewriter;
  bool isLocValid;
  Arguments &args;
  
};

class FunctionDeclFormatter {
public:
  FunctionDecl *funcDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, Arguments &arg);
  string getParamsNamesASString();
  string getParamsDeclASString();
  string getFortranFunctDeclASString();
  string getParamsTypesASString();
  bool argLocValid();

private:
  QualType returnQType;
  llvm::ArrayRef<ParmVarDecl *> params;
  Rewriter &rewriter;
  Arguments &args;
};


class MacroFormatter {
public:
  const MacroDirective *md;
  string macroName;
  string macroVal;
  string macroDef;
  bool isInSystemHeader;
  PresumedLoc sloc;

  MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci, Arguments &arg);
  bool isObjectLike();
  bool isFunctionLike();
  string getFortranMacroASString();


private:
  bool isObjectOrFunction;
  Arguments &args;
  //CompilerInstance &ci;
};


//------------Visitor class decl----------------------------------------------------------------------------------------------------

// Main class which works to translate the C to Fortran by calling helpers.
class TraverseNodeVisitor : public RecursiveASTVisitor<TraverseNodeVisitor> {
public:
  TraverseNodeVisitor(Rewriter &R, Arguments& arg) : TheRewriter(R), args(arg) {}


  bool TraverseDecl(Decl *d);
  bool TraverseStmt(Stmt *x);
  bool TraverseType(QualType x);
  string allFunctionDecls;

private:
  Rewriter &TheRewriter;
  // Additional translation arguments (ie quiet/silent)
  Arguments &args;
};

// Traces the preprocessor as it moves through files and records the inclusions in a stack
// using a set to keep track of files already seen
class TraceFiles : public PPCallbacks {
public:
  TraceFiles(CompilerInstance &ci, std::set<string>& filesseen, std::stack<string>& filesstack, Arguments& arg) :
  ci(ci), seenfiles(filesseen), stackfiles(filesstack), args(arg) { }

  // Writes into the stack if a new file is entered by the preprocessor
  void FileChanged(clang::SourceLocation loc, clang::PPCallbacks::FileChangeReason reason,
        clang::SrcMgr::CharacteristicKind filetype, clang::FileID prevfid) override {

    // We have found a system header and have been instructured to skip it, so we move along
    if (ci.getSourceManager().isInSystemHeader(loc) == true && args.getNoSystemHeaders() == true) {
      return;
    }
    clang::PresumedLoc ploc = ci.getSourceManager().getPresumedLoc(loc);
    string filename = ploc.getFilename();
    if (seenfiles.find(filename) != seenfiles.end()) {
     } else {
      seenfiles.insert(filename);
      stackfiles.push(filename);
    }
  }

private:
  CompilerInstance &ci;
  // Recording data structures to keep track of files the preprocessor sees
  std::set<string>& seenfiles;
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
// of USE statements.
class CreateHeaderStackAction : public clang::ASTFrontendAction {
public:
  CreateHeaderStackAction(std::set<string>& filesseen, std::stack<string>& filesstack, Arguments &arg) :
     seenfiles(filesseen), stackfiles(filesstack), args(arg) {} 

  // When a source file begins, the callback to trace filechanges is registered
  // so that all changes are recorded.
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
  std::set<string>& seenfiles;
  std::stack<string>& stackfiles; 
  Arguments &args;
};

// Factory to run the preliminary preprocessor file tracing;
// determines the order to recursively translate header files
// when writing modules recursively
class CHSFrontendActionFactory : public FrontendActionFactory {
public:
  CHSFrontendActionFactory(std::set<string>& seenfiles, std::stack<string>& stackfiles, Arguments &arg) :
     seenfiles(seenfiles), stackfiles(stackfiles), args(arg) {} 

  // Creates a new action which only attends to file changes in the preprocessor.
  // This allows tracing the files included.
  CreateHeaderStackAction *create() override {
    return new CreateHeaderStackAction(seenfiles, stackfiles, args);
  }

private:
  // Set to keep track of all the files we have seen
  std::set<string>& seenfiles;
  // Stack to keep track of the order for translation of files
  std::stack<string>& stackfiles;
  // Additional arguments
  Arguments &args;
};
  
// Classes, specifications, etc for the main translation program!
//-----------PP Callbacks functions----------------------------------------------------------------------------------------------------
// Class used by the main TNActions to inspect and translate C macros
class TraverseMacros : public PPCallbacks {
public:

  explicit TraverseMacros(CompilerInstance &ci, Arguments &arg)
  : ci(ci), args(arg) {}//, SM(ci.getSourceManager()), pp(ci.getPreprocessor()),

  // Call back to translate each macro when it is defined
  void MacroDefined (const Token &MacroNameTok, const MacroDirective *MD); 
private:
  CompilerInstance &ci;
  // Additional arguments
  Arguments &args;
};

  //-----------the main program----------------------------------------------------------------------------------------------------

class TraverseNodeConsumer : public clang::ASTConsumer {
public:
  TraverseNodeConsumer(Rewriter &R, Arguments &arg) : Visitor(R, arg), args(arg)  {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context);

private:
// A RecursiveASTVisitor implementation.
  TraverseNodeVisitor Visitor;
  // Additional arguments
  Arguments &args;
};

// Main translation action to be carried out on a C header file
class TraverseNodeAction : public clang::ASTFrontendAction {
public:

  TraverseNodeAction(string to_use, Arguments &arg) :
       use_modules(to_use), args(arg) {}

  // // for macros inspection
  bool BeginSourceFileAction(CompilerInstance &ci, StringRef Filename) override;

  // Action at the completion of a source file traversal
  void EndSourceFileAction() override;

  // Returns an AST consumer which does the majority of the translation work
  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
    return llvm::make_unique<TraverseNodeConsumer>(TheRewriter, args);
  }

private:
  Rewriter TheRewriter;
  string fullPathFileName;
  // Modules to include in USE statements in this file's module
  string use_modules;
  // Additional arguments
  Arguments &args;
};

// Clang tools run FrontendActionFactories which implement
// a method to return a new Action for each file
class TNAFrontendActionFactory : public FrontendActionFactory {
public:
  TNAFrontendActionFactory(string to_use, Arguments &arg) :
     use_modules(to_use), args(arg) {};

  // Mandatory function to create a file's TNAction
  TraverseNodeAction *create() override {
    return new TraverseNodeAction(use_modules, args);
  }

private:
  // Modules previously written to be included in USE statements
  string use_modules;
  // Additional arguments (ie quiet/silent)
  Arguments &args;
};


