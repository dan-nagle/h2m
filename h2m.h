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


#include <stdio.h>
#include <string>
#include <sstream>
#include <set>
#include <deque>
#include <stack>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

// Option to substitute symbols in the C headers for something else
static cl::opt<string> Subs("substitute", cl::desc("<symbol_present>:<symbol_to_subsitute>"));

//------------Formatter class decl----------------------------------------------------------------------------------------------------
class CToFTypeFormatter {
public:
  QualType c_qualType;
  ASTContext &ac;
  PresumedLoc sloc;

  CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc sloc);
  string getFortranTypeASString(bool typeWrapper);
  string getFortranIdASString(string raw_id);
  bool isSameType(QualType qt2);
  static bool isIntLike(const string input);
  static bool isDoubleLike(const string input);
  static bool isType(const string input);
  static bool isString(const string input);
  static bool isChar(const string input);
  static string createFortranType(const string macroName, const string macroVal, PresumedLoc loc);
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
  RecordDeclFormatter(RecordDecl *rd, Rewriter &r);
  void setMode();
  void setTagName(string name);
  bool isStruct();
  bool isUnion();
  string getFortranStructASString();
  string getFortranFields();



private:
  Rewriter &rewriter;
  
};

class EnumDeclFormatter {
public:
  EnumDecl *enumDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  EnumDeclFormatter(EnumDecl *e, Rewriter &r);
  string getFortranEnumASString();

private:
  Rewriter &rewriter;
  
};

class VarDeclFormatter {
public:
  VarDecl *varDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  VarDeclFormatter(VarDecl *v, Rewriter &r);
  string getInitValueASString();
  string getFortranVarDeclASString();
  string getFortranArrayDeclASString();
  void getFortranArrayEleASString(InitListExpr *ile, string &arrayValues, string arrayShapes, bool &evaluatable, bool firstEle);

private:
  Rewriter &rewriter;
  string arrayShapes_fin;
  
};

class TypedefDeclFormater {
public:
  TypedefDecl *typedefDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  TypedefDeclFormater(TypedefDecl *t, Rewriter &r);
  string getFortranTypedefDeclASString();

private:
  Rewriter &rewriter;
  bool isLocValid;
  
};

class FunctionDeclFormatter {
public:
  FunctionDecl *funcDecl;
  bool isInSystemHeader;
  PresumedLoc sloc;

  // Member functions declarations
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r);
  string getParamsNamesASString();
  string getParamsDeclASString();
  string getFortranFunctDeclASString();
  string getParamsTypesASString();
  bool argLocValid();

private:
  QualType returnQType;
  llvm::ArrayRef<ParmVarDecl *> params;
  Rewriter &rewriter;
};


class MacroFormatter {
public:
  const MacroDirective *md;
  string macroName;
  string macroVal;
  string macroDef;
  bool isInSystemHeader;
  PresumedLoc sloc;

  MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci);
  bool isObjectLike();
  bool isFunctionLike();
  string getFortranMacroASString();


private:
  bool isObjectOrFunction;
  //CompilerInstance &ci;
};


//------------Visitor class decl----------------------------------------------------------------------------------------------------

class TraverseNodeVisitor : public RecursiveASTVisitor<TraverseNodeVisitor> {
public:
  TraverseNodeVisitor(Rewriter &R, llvm::tool_output_file &out) : TheRewriter(R), output(out) {}


  bool TraverseDecl(Decl *d);
  bool TraverseStmt(Stmt *x);
  bool TraverseType(QualType x);
  string allFunctionDecls;

private:
  Rewriter &TheRewriter;
  llvm::tool_output_file &output;
};

// Traces the preprocessor as it moves through files and records the inclusions in a stack
class TraceFiles : public PPCallbacks {
public:
  TraceFiles(CompilerInstance &ci, std::set<string>& filesseen, std::stack<string>& filesstack) :
  ci(ci), seenfiles(filesseen), stackfiles(filesstack) { errs() << "Tracefiles created \n";}

  void FileChanged(clang::SourceLocation loc, clang::PPCallbacks::FileChangeReason reason,
        clang::SrcMgr::CharacteristicKind filetype, clang::FileID prevfid) override {
    clang::PresumedLoc ploc = ci.getSourceManager().getPresumedLoc(loc);
    string filename = ploc.getFilename();
    if (seenfiles.find(filename) != seenfiles.end()) {
      errs() << "Skipping old file " << filename << "\n";
     } else {
      errs() << "Entering new file " << filename << "\n";
      seenfiles.insert(filename);
      stackfiles.push(filename);
    }
  }

private:
  CompilerInstance &ci;
  std::set<string>& seenfiles;
  std::stack<string>& stackfiles;
};

// Empty. I'm just trying to get this to work. I have no need of this other
// than to avoid other issues.
class InactiveNodeConsumer : public clang::ASTConsumer {
public:
  InactiveNodeConsumer() {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context) {}
};

// Action to follow the preprocessor and create a stack of files to be dealt with
class CreateHeaderStackAction : public clang::ASTFrontendAction {
public:
  CreateHeaderStackAction(std::set<string>& filesseen, std::stack<string>& filesstack) :
     seenfiles(filesseen), stackfiles(filesstack) {}

  bool BeginSourceFileAction(CompilerInstance &ci, StringRef Filename) override {
    Preprocessor &pp = ci.getPreprocessor();
    pp.addPPCallbacks(llvm::make_unique<TraceFiles>(ci, seenfiles, stackfiles));
    return true;
   }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    return llvm::make_unique<InactiveNodeConsumer>();
  }

private:
  std::set<string>& seenfiles;
  std::stack<string>& stackfiles; 
};

// Factory to run the preliminary preprocessor file tracing
class CHSFrontendActionFactory : public FrontendActionFactory {
public:
  CHSFrontendActionFactory(std::set<string>& seenfiles, std::stack<string>& stackfiles) :
     seenfiles(seenfiles), stackfiles(stackfiles) {} 

  CreateHeaderStackAction *create() override {
    errs() << "Creating new header stack action \n";
    return new CreateHeaderStackAction(seenfiles, stackfiles);
  }

private:
  std::set<string>& seenfiles;
  std::stack<string>& stackfiles;
};
  
// Classes, specifications, etc for the main translation program!
//-----------PP Callbacks functions----------------------------------------------------------------------------------------------------
class TraverseMacros : public PPCallbacks {
  CompilerInstance &ci;
  // SourceManager& SM;// = ci.getSourceManager();
  // Preprocessor &pp; // = ci.getPreprocessor();
  // int Indent;
  // llvm::formatted_raw_ostream FOuts;
public:

  explicit TraverseMacros(CompilerInstance &ci, llvm::tool_output_file &out)
  : ci(ci), output(out) {}//, SM(ci.getSourceManager()), pp(ci.getPreprocessor()),
  //Indent(0), FOuts((*output_ref).os()) {}

  void MacroDefined (const Token &MacroNameTok, const MacroDirective *MD); 
private:
  llvm::tool_output_file &output;
};

  //-----------the main program----------------------------------------------------------------------------------------------------

class TraverseNodeConsumer : public clang::ASTConsumer {
public:
  TraverseNodeConsumer(Rewriter &R, llvm::tool_output_file &out) : Visitor(R, out), output(out)  {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context);

private:
// A RecursiveASTVisitor implementation.
  TraverseNodeVisitor Visitor;
  llvm::tool_output_file &output;
};

class TraverseNodeAction : public clang::ASTFrontendAction {
public:

  TraverseNodeAction(string to_use, llvm::tool_output_file &out) :
       use_modules(to_use), output(out) {}

  // // for macros inspection
  bool BeginSourceFileAction(CompilerInstance &ci, StringRef Filename) override;

  void EndSourceFileAction() override;

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
    return llvm::make_unique<TraverseNodeConsumer>(TheRewriter, output);
  }

private:
  Rewriter TheRewriter;
  string fullPathFileName;
  string use_modules;
  llvm::tool_output_file &output;
};

class TNAFrontendActionFactory : public FrontendActionFactory {
public:
  TNAFrontendActionFactory(string to_use, llvm::tool_output_file &out) :
     use_modules(to_use), output(out) {};

  TraverseNodeAction *create() override {
    return new TraverseNodeAction(use_modules, output);
  }

private:
  string use_modules;
  llvm::tool_output_file &output; 
};




