// The following file contains the source code for the VarDeclFormatter
// for the h2m translator. This includes handling of regular variable
// declarations as well as structs and arrays.

#include "h2m.h"

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

// -----------initializer VarDeclFormatter--------------------
VarDeclFormatter::VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  varDecl = v;
  // Because sloc is checked for validity prior to use, this should handle invalid locations. If it
  // isn't initialized, it isn't valid according to the Clang function check.
  if (varDecl->getSourceRange().getBegin().isValid()) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(varDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(varDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header.
  }
};

// In the event that a variable declaration has an initial value, this function
// attempts to find that initialization value and return it as a string. It handles
// pointers, reals, complexes, characters, ints. Arrays are defined here but actually
// handled in their own function. Typedefs and structs are not handled here.
string VarDeclFormatter::getInitValueASString() {
  string valString;

  // This prevents sytstem files from leaking in to the translation.
  if (varDecl->hasInit() && !isInSystemHeader) {
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

      } else {  // We don't recognize and can't handle this declaration.
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
  return valString;  // This is empty if there was no initialization.

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
      // Evaluate the expression as an 'r' value using any crazy technique the Clang designers
      // want to use. This doesn't necessarilly follow the language standard.
      innerelement->EvaluateAsRValue(r, varDecl->getASTContext());
      string eleVal = r.Val.getAsString(varDecl->getASTContext(), innerelement->getType());
      // We must convert this integer string into a char. This is annoying.
      if (is_char == true) {
        // This will throw an exception if it fails but it should succeed.
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
    // Recursively calls to find the smallest element of the array if
    // it is multidimensional (in which case elements are themselves
    // some form of array).
    } else if (isa<InitListExpr> (innerelement)) {
      InitListExpr *innerile = cast<InitListExpr> (innerelement);
      getFortranArrayEleASString(innerile, arrayValues, arrayShapes, evaluatable,
          (it == innerElements.begin()) && firstEle, is_char);
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
  if (varDecl->getType().getTypePtr()->isArrayType() && !isInSystemHeader) {
    CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
    // If asked to autobind, this string holds the identifier to bind to
    // it is initialized as empty but may contain a name later.
    string bindname = "";
    string identifier = varDecl->getNameAsString();

    // Check for an illegal name length. Warn if it exists.
    CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);

    // Illegal underscore is found in the array declaration
    if (identifier.front() == '_') {
      // If necessary, prepare a bind name to properly link to the C function
      if (args.getAutobind() == true) {
        // This is the proper syntax to bind to a C variable: BIND(C, name="cname")
        bindname = " , name =\"" + identifier + "\"";
      }
      if (args.getSilent() == false) {
        errs() << "Warning: illegal array identifier " << identifier;
        errs() << " renamed h2m" << identifier << ".\n";
        LineError(sloc);
      }
      identifier = "h2m" + identifier;
    }

    // Check to see whether we have seen this identifier before. If need be, comment
    // out the duplicate declaration.
    // We need to strip off the (###) for the test or we will end up testing
    // for a repeat of the variable n(4) rather than n if the decl is int n[4];
    if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << identifier;
        errs() << ", array declaration.\n";
        LineError(sloc);
      }
      arrayDecl = "! Skipping duplicate declaration of " + identifier + "\n!";
    }

    if (!varDecl->hasInit()) {
      // only declared, no initialization of the array takes place
      // Note that the bindname will be empty unless certain options are in effect and
      // the function's actual name is illegal.
      arrayDecl += tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname + ") :: ";
      arrayDecl += tf.getFortranIdASString(identifier) + "\n";
    } else {
      // The array is declared and initialized. We must tranlate the initialization.
      const ArrayType *at = varDecl->getType().getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType ();
      Expr *exp = varDecl->getInit();
      // Whether this is a char array or not will have to be checked several times.
      // This checks whether or not the "innermost" type is a char type.
      bool isChar = varDecl->getASTContext().getBaseElementType(e_qualType).getTypePtr()->isCharType();
      // This fetches the actual text initialization, which may be a string literal or {'a', 'b'...}.
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc(),
            varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
     // A char array might not be a string literal. A leading { character indicates this case.
     if (isChar == true && arrayText.front() != '{') {
        // handle stringliteral case
        // A parameter may not have a bind(c) attribute (static/dynamic storage do not interoperate)
        arrayDecl += tf.getFortranTypeASString(true) + ", parameter, public :: " +
            tf.getFortranIdASString(identifier) + " = " + arrayText + "\n";
      } else {  // This is not a string literal but a standard C array.
        bool evaluatable = false;
        Expr *exp = varDecl->getInit();
        if (isa<InitListExpr> (exp)) {
          // initialize shape (dimensions) and values
          // format: INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/))
          string arrayValues;
          string arrayShapes;

          // Get the initialization values in an array form from the Expr.
          InitListExpr *ile = cast<InitListExpr>(exp);
          ArrayRef<Expr *> elements = ile->inits();
          size_t numOfEle = elements.size();
          // This will be the first dimension of the array.
          arrayShapes = to_string(numOfEle);
          arrayShapes_fin = arrayShapes;
          // Set up an iterator to cycle through all the elements.
          for (auto it = elements.begin(); it != elements.end(); it++) {
            Expr *element = (*it);
            if (isa<InitListExpr> (element)) {
              // This is a multidimensional array; elements are arrays themselves.
              InitListExpr *innerIle = cast<InitListExpr>(element);
              // This function will recursively find the dimensions. The arrayValues and
              // arrayShapes are initialized in the function (strings are passed by reference)..
              getFortranArrayEleASString(innerIle, arrayValues, arrayShapes, evaluatable,
                  it == elements.begin(), isChar);
            } else {
              if (element->isEvaluatable(varDecl->getASTContext())) {
                // This is a one dimensional array. Elements are scalars.
                clang::Expr::EvalResult r;
                // The expression is evaluated (operations are performed) to give the final value.
                element->EvaluateAsRValue(r, varDecl->getASTContext());
                string eleVal = r.Val.getAsString(varDecl->getASTContext(), e_qualType);

                // In the case of a char array initalized ie {'a', 'b',...} we require a 
                // check and a conversion of the int value produced into a char value which
                // is valid in a Fortran character array.
                if (isChar == true) {
                  // Transfering the arbitrary percision int to a string then back to an int is
                  // easier than trying to get an integer from the arbitrary precision value.
                  // On a failed conversion, this throws an exception. That shouldn't happen.
                  int temp_val = std::stoi(eleVal);
                  char temp_char = static_cast<char>(temp_val);
                  // Assembles the necessary syntax for a Fortran char array, a component is: 'a'...
                  eleVal = "'";
                  eleVal += temp_char;
                  eleVal += "'";
                }

                if (it == elements.begin()) {
                  // Initialize the string on the first element.
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
            // comment out arrayText using the string stream
            std::istringstream in(arrayText);
            for (std::string line; std::getline(in, line);) {
              arrayDecl += "! " + line + "\n";
              if (args.getQuiet() == false && args.getSilent() == false) {
                errs() << "Warning: array text " << line << " commented out.\n";
                LineError(sloc);
              }
            }
          } else {
            // The array is evaluatable and has been evaluated already. We assemble the
            //  declaration. INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /),
            //  (/2, 3/)). bindname may be empty.
            arrayDecl += tf.getFortranTypeASString(true)+", BIND(C" + bindname +  ") :: "+
                identifier +"("+arrayShapes_fin+")";
            arrayDecl += " = RESHAPE((/"+arrayValues+"/), (/"+arrayShapes_fin+"/))\n";
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
  string bindname = "";  // May eventually hold a value to bind to.
  
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

      if (identifier.front() == '_') {
         if (args.getSilent() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         if (args.getAutobind() == true) {  // Set up the bind phrase if requested.
           // The proper syntax is BIND(C, name="cname").
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      }
      // Assemble the variable declaration, including the bindname if it exists.
      vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname;
      vd_buffer +=  ") :: " + identifier + "\n";

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
        // handle initialized numeric array specifically in its own functions.
        // length, name, and identifier repetition checks are carried out in the helper
        vd_buffer = getFortranArrayDeclASString();
    } else if (varDecl->getType().getTypePtr()->isPointerType() && 
      varDecl->getType().getTypePtr()->getPointeeType()->isCharType()) {
      // string declaration
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType().getTypePtr()->getPointeeType(),
          varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      // Check for an illegal character at the string identifier's start.
      if (identifier.front() == '_') {
         if (args.getSilent() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            LineError(sloc);
         }
         if (args.getAutobind() == true) {  // Setup the autobinding buffer if requested
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      }
      // Detrmine the state of the declaration. Is there something declared? Is it commented out?
      // Create the declaration in correspondence with this. Add in the bindname, which may be empty
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname;
        vd_buffer +=  ") :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname;
        vd_buffer +=  + ") :: " + identifier + " " + value + "\n";
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
         if (args.getAutobind() == true) {
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      } 
      // Determine the state of the declaration and assemble the appropriate Fortran equivalent.
      // Include the bindname, which may be empty.
      if (value.empty()) {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname;
        vd_buffer += ") :: " + identifier + "\n";
      } else if (value[0] == '!') {
        vd_buffer = tf.getFortranTypeASString(true) + ", public, BIND(C" + bindname;
        vd_buffer = ") :: " + identifier + " " + value + "\n";
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

