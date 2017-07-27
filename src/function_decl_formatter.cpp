// Here is the definition of the formatter class used by
// h2m to translate functions from C to fortran.

#include "h2m.h"

// -----------initializer FunctionDeclFormatter--------------------
FunctionDeclFormatter::FunctionDeclFormatter(FunctionDecl *f, Rewriter &r, 
    Arguments &arg) : rewriter(r), args(arg) {
  error_string = "";
  funcDecl = f;
  current_status = CToFTypeFormatter::OKAY;
  returnQType = funcDecl->getReturnType();
  params = funcDecl->parameters();
  // Because sloc is checked for validity prior to use, this should be a fine way to deal with
  // invalid locations
  if (funcDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(
        funcDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        funcDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
};

// For inserting types to "USE iso_c_binding, only: <<< c_ptr, c_int>>>""
// This function determines the types which are passed into a function so that
// the above demonstrated syntax can be used to establish proper fortran binding.
// Each type present will only be mentioned once.
string FunctionDeclFormatter::getParamsTypesASString() {
  string paramsType = "";
  std::set<string> param_types;
  // loop through all arguments of the function, determine 
  // their types, and add them into the set.
  for (auto it = params.begin(); it != params.end(); it++) {
    CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(),
        sloc, args);
    bool problem = false;
    // The flag will indicate a bad type.
    string type_no_wrapper = tf.getFortranTypeASString(false, problem);
    if (problem == true) {
      current_status = CToFTypeFormatter::BAD_TYPE;
      error_string = type_no_wrapper + ", function argument.";
    }
    // If we have a valid type to add, begin the argument list!
    // This method of determining validity is left over from
    // the original function (which I rewrote -Michelle). It is
    // use to keep from including structured types in the bind
    // lists.
    if (type_no_wrapper.find("C_") != std::string::npos) {
      param_types.insert(type_no_wrapper);
    }
  }
  // Now that we have found the type of the arguments, find the return
  // type, too. Deal with the potential of a void (subroutine) return. 
  CToFTypeFormatter rtf(returnQType, funcDecl->getASTContext(), sloc, args);
  if (!returnQType.getTypePtr()->isVoidType()) {
    bool problem = false;
    string return_type = rtf.getFortranTypeASString(false, problem);
    if (problem == true) {  // We have found an invalid type
      current_status = CToFTypeFormatter::BAD_TYPE;
      error_string = return_type + ", function return type.";
    }  
    // If the return type is valid, add it into the list with
    // the appropriate syntax. 
    if (return_type.find("C_") != std::string::npos) {
      param_types.insert(return_type);
    }
  }
  // We have placed all the types as strings in a set. Now we iterate
  // through the set and write them into the string of param types.
  for (std::set<string>::iterator iter = param_types.begin(); iter != 
      param_types.end(); ++iter) {
    paramsType += *iter + ", ";
  }
  // Erase the extra ", " added on in the last iteration.
  // If there were no arguments and it was a subroutine, the
  // string may be empty.
  if (paramsType.length() > 2) {  // Protect against an empty string.
    paramsType.erase(paramsType.end() - 2, paramsType.end());
  }
  return paramsType;
};

// For inserting variable decls "<<<type(c_ptr), value :: arg_1>>>"
// This function gives the parameters passed to the function in 
// the form needed after the initial function declaration to 
// specify their types, intents, etc.
// A CToFTypeFormatter is created for each as the function loops
// through all the parameters.
string FunctionDeclFormatter::getParamsDeclASString() { 
  string paramsDecl;
  int index = 1;
  for (auto it = params.begin(); it != params.end(); it++) {
    // If the param name is empty, rename it to arg_index
    string pname = (*it)->getNameAsString();
    if (pname.empty()) {
      pname = "arg_" + to_string(index);
    }
    if (pname.front() == '_') {  // Illegal character. Append a prefix.
      string old_pname = pname;
      pname = "h2m" + pname;
    }
    // Check for a valid name length for the dummy variable.
    if (pname.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = pname + ", function parameter.";
    }
    
    CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(),
        sloc, args);

    // Array arguments must be handled diferently. They need the DIMENSION attribute.
    if (tf.isArrayType() == true) {
      paramsDecl += "    " + tf.getFortranArrayArgASString(pname) + "\n";
    } else {
      // In some cases parameter doesn't have a name in C, but must have one by
      //  the time we get here.
      bool problem = false;
      string type_wrapped = tf.getFortranTypeASString(true, problem);
      if (problem == true) {  // We have seen an unrecognized type
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = type_wrapped + ", parameter type.";
      }
      paramsDecl += "    " + type_wrapped + ", value" + " :: " + pname + "\n";
      // need to handle the attribute later - Michelle doesn't know what this 
      // (original) commment means 
    }
    // Similarly, check the length of the declaration line to make sure it is valid Fortran.
    // Note that the + 1 in length is to account for the newline character.
    if (pname.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = pname + ", parameter name.";
    }
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
  // We cycle through the parameters, assuming the type by loose binding.
  for (auto it = params.begin(); it != params.end(); it++) {
    if (it == params.begin()) {
      // if the param name is empty, rename it to arg_index
      //uint64_t  getTypeSize (QualType T) const for array!!!
      // -Michelle doesn't know what that second line means.
      string pname = (*it)->getNameAsString();
      if (pname.empty()) {
        pname = "arg_" + to_string(index);
      }
      if (pname.front() == '_') {  // Illegal character. Append a prefix.
        // Note we only call this here to avoid multiple warnings.
        CToFTypeFormatter::PrependError(pname, args, sloc);
        pname = "h2m" + pname;
      }
      paramsNames += pname;
    } else { // parameters in between
      // if the param name is empty, rename it to "arg_index"
      string pname = (*it)->getNameAsString();
      if (pname.empty()) {
        pname = "arg_" + to_string(index);
      }
      if (pname.front() == '_') {  // Illegal character. Append a prefix.
        CToFTypeFormatter::PrependError(pname, args, sloc);
        pname = "h2m" + pname;
      }
      paramsNames += ", " + pname; 
    }
    index++;
  }
  return paramsNames;
};

// This simply determines whether or not the location of
// all arguments are valid and returns true if so.
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
// on helpers to obtain names and types of arguments.
string FunctionDeclFormatter::getFortranFunctDeclASString() {
  string fortranFunctDecl;
  // This prevents sytem headers from leaking into the translation. It also
  // keeps out invalid arugment locations.
  if (!isInSystemHeader && argLocValid()) {
    string funcType;
    string paramsString = getParamsTypesASString();
    string imports;
    string bindname;  // This is used to link to a C function with a different name.
    // This determines what types to be included in the iso_c_binding.
    if (!paramsString.empty()) {
      imports = "    USE iso_c_binding, only: " + getParamsTypesASString() + "\n";
    } else {
      imports = "    USE iso_c_binding\n";
    }
    imports +="    import\n";
    
    // Check if the return type is void or not
    // A void type means we create a subroutine. Otherwise a function is written.
    if (returnQType.getTypePtr()->isVoidType()) {
      funcType = "SUBROUTINE";
    } else {
      CToFTypeFormatter tf(returnQType, funcDecl->getASTContext(), sloc, args);
      bool problem = false;
      funcType = tf.getFortranTypeASString(true, problem) + " FUNCTION";
      if (problem == true) {  // An invalid type of some sort has been found
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = funcType + ", parameter type.";
      }
    }
    string funcname = funcDecl->getNameAsString();
    if (funcname.front() == '_') {  // We have an illegal character in the identifier
      CToFTypeFormatter::PrependError(funcname, args, sloc);
      // If necessary, prepare a bind name to properly link to the C function
      // because we have been forced to change this function's declared name.
      if (args.getAutobind() == true) {
        // This is the proper syntax to bind to a C variable: BIND(C, name="cname")
        bindname = ", name =\"" + funcname + "\"";
      }
      funcname = "h2m" + funcname;  // Prepend h2m to fix the problem
    }
    // Check to make sure the function's name isn't too long. 
    if (funcname.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = funcname + ", function name.";
    }
    // Check to make sure this declaration line isn't too long. It well might be.
    // bindname may be empty or may contain a C function to link to.
    fortranFunctDecl = funcType + " " + funcname + "(" + getParamsNamesASString() +
        ")" + " BIND(C" + bindname + ")\n";
    // Add in the import from iso_c_binding and the parameters.
    fortranFunctDecl += imports;
    fortranFunctDecl += getParamsDeclASString();
    // preserve the function body as comment
    if (funcDecl->hasBody()) {
      Stmt *stmt = funcDecl->getBody();
      clang::SourceManager &sm = rewriter.getSourceMgr();
      // comment out the entire function {!body...}
      string bodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(
          stmt->getSourceRange()),
          sm, LangOptions(), 0);
      string commentedBody;
      std::istringstream in(bodyText);
      // Unless told to be silent or quiet, inform the user that the
      // lines have been commented out.
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line " << line << " commented out \n";
          CToFTypeFormatter::LineError(sloc);
        }
        commentedBody += "! " + line + "\n";
      }
      fortranFunctDecl += commentedBody;

    }
    // Close the function or subroutine as appropriate.
    if (returnQType.getTypePtr()->isVoidType()) {
      fortranFunctDecl += "END SUBROUTINE " + funcname + "\n\n";   
    } else {
      fortranFunctDecl += "END FUNCTION " + funcname + "\n\n";
    }
   
    // The guard function checks for duplicate identifiers. This might 
    // happen because C is case sensitive. It shouldn't happen often, but if
    // it does, the duplicate declaration needs to be commented out.
    bool duplicate = RecordDeclFormatter::StructAndTypedefGuard(funcname); 
    if (duplicate == false) {  // This implies this is a repeat.
      current_status = CToFTypeFormatter::DUPLICATE;
      error_string = funcname + ", function name.";
    }

    // We check the line lengths in one place to make sure they are
    // all valid fortran lengths.
    std::istringstream in(fortranFunctDecl);
    for (std::string line; std::getline(in, line);) {
      if (line.length() > CToFTypeFormatter::line_max) {
        current_status = CToFTypeFormatter::BAD_LINE_LENGTH; 
        error_string = line + ", in function.";
      }
    }
  }

  return fortranFunctDecl;
};

