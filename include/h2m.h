// The formatters.h file includes definitions of all the type and
// declaration formatters used in translation.
// The classes used to traverse the tree are defined here.
// These include the classes used to trace the preprocessor's
// progress to allow a recursive run and the main action used
// to perform translations.

// formatters.h includes various needed Clang/LLVM and C/C++ library
// header files. It also defines the formatter classes used to
// translate C to Fortran.
#include "formatters.h"

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


