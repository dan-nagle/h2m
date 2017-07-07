// Here are the definition of formatter classes for the h2m
// autofortran tool including RecordDeclFormatter, VarDeclFormatter,
// FunctDeclFormatter, and EnumDeclFormatter.

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

// This function exists to make sure that a type is not declared
// twice. This frequently happens with typedefs renaming structs.
// This results in duplicate-name-declaration errors in Fortran
// because there are no seperate name-look-up procedures for
// "tag-names" as there are in C (ie in C typedef struct Point point
// is legal but in Fortran this causes conflicts.) A static map will
// make sure that no typedef declares an already declared name.
// If the name has already been seen, it returns false. If it hasn't,
// it adds it to a set (will return false if called again with that
// name) and returns true.
bool RecordDeclFormatter::StructAndTypedefGuard(string name) {
  static std::set<string> seennames;  // Records all identifiers seen.

  // Insurance. The name should always have been prepended prior to this call,
  // but this makes sure that all names are prepended prior to the check.
  if (name.front() == '_') { 
    name = "h2m" + name;
  }

  // Put the name into lowercase. Fortran is not case sensitive.
  std::locale location;  // Determines if a lowercase exists
  for (string::size_type j = 0; j < name.length(); j++) {
    name[j] = std::tolower(name[j], location); 
  }

  // If we have already added this file to the set... false
  if (seennames.find(name) != seennames.end()) {
    return false;
  } else {  // Otherwise, add it to the stack and return true
    seennames.insert(name);
    return true;
  }
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
    // Recursively calls to find the smallest element of the array
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
          } else {  // The array is evaluatable and has been evaluated already. We assemble the declaration.
            //INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/)). bindname may be empty.
            arrayDecl += tf.getFortranTypeASString(true)+", BIND(C" + bindname +  ") :: "+ identifier +"("+arrayShapes_fin+")";
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

// -----------initializer Typedef--------------------
TypedefDeclFormater::TypedefDeclFormater(TypedefDecl *t, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  typedefDecl = t;
  isLocValid = typedefDecl->getSourceRange().getBegin().isValid();
  // sloc, if uninitialized, will be an invalid location. Because it is always passed to a function
  // which checks validity, this should be a fine way to guard against an invalid location.
  if (isLocValid) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(typedefDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(typedefDecl->getSourceRange().getBegin());
  }  
  
};

// From a C typedef, a string representing a Fortran pseudo-typedef is created. The fortran equivalent
// is a type with only one field. The name of this field is name_type (ie name_C_INT), depending
// on the type. Note that this function will be called for Structs and Enums as well, but that they
// will be skipped and handled elsewhere (in recordDeclFormatter). A typedef with an illegal 
// name will be prepended with "h2m" to become legal fortran.
// This function will check for name duplication. In the case that this is a duplicate identifier,
// a string containing a comment will be returned (no definition will be provided).
// Note that no bindname is allowed because BIND(C, name="") is not permitted in a TYPE.
string TypedefDeclFormater::getFortranTypedefDeclASString() {
  string typdedef_buffer = "";
  if (isLocValid && !isInSystemHeader) {  // Keeps system files from leaking in
  // if (typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isStructureType()
  //  or typedefDecl->getTypeSourceInfo()->getType().getTypePtr()->isEnumeralType ()) {
  // } else {
  // The above commented out section appeared with a comment indicating that struct/enum
  // typedefs would be handled in the RecordDeclFormatter section. This does not appear
  // to be the case.
    TypeSourceInfo * typeSourceInfo = typedefDecl->getTypeSourceInfo();
    CToFTypeFormatter tf(typeSourceInfo->getType(), typedefDecl->getASTContext(), sloc, args);
    string identifier = typedefDecl->getNameAsString();
    if (identifier.front() == '_') {  // This identifier has an illegal _ at the begining.
      if (args.getSilent() == false) {  // Warn about the renaming unless silenced.
        errs() << "Warning: illegal identifier " << identifier << " renamed h2m" << identifier << "\n";
        LineError(sloc);
      }
      identifier = "h2m" + identifier;  // Prepen dh2m to fix the problem.
    }
    

    // Check to make sure the identifier is not too lone
    CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Include the bindname, which may be empty, when assembling the definition.
    typdedef_buffer = "TYPE, BIND(C) :: " + identifier + "\n";
    // Because names in typedefs may collide with the typedef name, 
    // suffixes are appended to the internal member of the typedef.
    if (args.getSilent() == false) {
      errs() << "Warning: due to name collisions during typdef translation, " << identifier;
      errs() <<  "\nrenamed " << identifier << tf.getFortranTypeASString(false) << "\n";
      LineError(sloc);
    }
    string to_add = "    "+ tf.getFortranTypeASString(true) + "::" + identifier+"_"+tf.getFortranTypeASString(false) + "\n";
    CheckLength(identifier + "_" + tf.getFortranTypeASString(false), CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check for an illegal length. The \n character is the reason for the +1. It doesn't count
    // towards line length.
    CheckLength(to_add, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
    typdedef_buffer += to_add;
    typdedef_buffer += "END TYPE " + identifier + "\n";
  //  }
    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary.
    if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
        LineError(sloc);
      }
      string temp_buf = typdedef_buffer;
      typdedef_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
      std::istringstream in(temp_buf);
      // Loops through the buffer like a line-buffered stream and comments it out
      for (std::string line; std::getline(in, line);) {
        typdedef_buffer += "! " + line + "\n";
      }
      
    }
  } 
  return typdedef_buffer;
};

// -----------initializer EnumDeclFormatter--------------------
EnumDeclFormatter::EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  enumDecl = e;
  // Becasue sloc is only ever passed to a function which checks its validity, this should be a fine
  // way to deal with an invalid location. An empty sloc is an invalid location.
  if (enumDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(enumDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(enumDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
};

// From a C enumerated type, a fotran ENUM is created. The names are prepended
// with h2m if they begin with an underscore. The function loops through all 
// the members in the enumerator and adds them into place. Note that this can
// resut in serious problems if the enumeration is too large. Note that there
// is no option to use a bind name here because it is not permitted to have
// a BIND(C, name="") statement in an Enum.
string EnumDeclFormatter::getFortranEnumASString() {
  string enum_buffer;
  bool anon = false;  // Lets us know if we need to comment out this declaration.

  if (!isInSystemHeader) {  // Keeps definitions in system headers from leaking into the translation
    string enumName = enumDecl->getNameAsString();

    if (enumName.empty() == true) {  // We don't have a proper name. We must get another form of identifier.
      enumName = enumDecl-> getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // This checks to make sure that this is not an anonymous enumeration
      if (enumName.find("anonymous at") != string::npos) {
        anon = true;  // Sets a bool to let us know that we have no name.
      }
    }

    if (enumName.front() == '_') {  // Illegal underscore beginning the name!
      if (args.getSilent() == false) {  // Warn unless silenced
        errs() << "Warning: illegal enumeration identifier " << enumName << " renamed h2m" << enumName << "\n";
        LineError(sloc); 
      } 
      enumName = "h2m" + enumName;  // Prepend h2m to fix the problem
    }

    // Check the length of the name to make sure it is valid Fortran
    // Note that it would be impossible for this line to be an illegal length unless
    // the variable name were hopelessly over the length limit. Note bindname may be empty.
    CheckLength(enumName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    if (anon == false) {
      enum_buffer = "ENUM, BIND(C) ! " + enumName + "\n";
    } else {  // Handle a nameless enum as best we can.
      enum_buffer = "ENUM, BIND(C)\n";
    }

    // enum_buffer += "    enumerator :: ";  // Removed when changes were made to allow unlimited enum length
    // Cycle through the pieces of the enum and translate them into fortran
    for (auto it = enumDecl->enumerator_begin (); it != enumDecl->enumerator_end (); it++) {
      string constName = (*it)->getNameAsString ();
      if (constName.front() == '_') {  // The name begins with an illegal underscore.
        string old_constName = constName;
        constName = "h2m" + constName;
        if (args.getSilent() == false) {
          errs() << "Warning: illegal enumeration identfier " << old_constName << " renamed ";
          errs() << constName << "\n";
          LineError(sloc);
        }
      }
      int constVal = (*it)->getInitVal().getExtValue();  // Get the initialization value
      // Check for a valid name length
      CheckLength(constName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
      // Problem! We have seen an identifier with this name before! Comment out the line
      // and warn about it.
      if (RecordDeclFormatter::StructAndTypedefGuard(constName) == false) { 
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << constName << ", enum member.\n";
          LineError(sloc);
        }
        enum_buffer += "! Skipping duplicate identifier.";
        enum_buffer +=  "    ! enumerator :: " + constName + "=" + to_string(constVal) + "\n";
      } else {  // Otherwise, just add it on the buffer
        enum_buffer += "    enumerator :: " + constName + "=" + to_string(constVal) + "\n";
      }
    }
    // erase the redundant colon  // This erasing and adding back in of a newline is obsolete with the new format
    // enum_buffer.erase(enum_buffer.size()-2);
    // enum_buffer += "\n";

    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary.
    if (RecordDeclFormatter::StructAndTypedefGuard(enumName) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << enumName << "\n";
        LineError(sloc);
      }
      // Comment out the declaration by stepping through and appending ! before newlines
      // to avoid duplicate identifier collisions.
      string temp_buf = enum_buffer;
      enum_buffer = "\n! Duplicate declaration of " + enumName + ", ENUM, skipped.\n";
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        enum_buffer += "! " + line + "\n";
      }
      // Add in a few last details... and return to avoid having END ENUM pasted on the end
      if (anon == false) {  // This should always be the case, but there's no harm in checks.
        enum_buffer += "! END ENUM !" + enumName + "\n";
      } else {
        enum_buffer += "! END ENUM\n";
      }
      return(enum_buffer);
    }

    if (anon == false) {  // Put the name after END ENUM unless there is no name.
      enum_buffer += "END ENUM !" + enumName + "\n";
    } else {
      enum_buffer += "END ENUM\n";
    }
  }

  return enum_buffer;
};

// -----------initializer RecordDeclFormatter--------------------

RecordDeclFormatter::RecordDeclFormatter(RecordDecl* rd, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  recordDecl = rd;
  // Because sloc is checked for validity prior to use, this should be a fine way to deal with
  // an invalid source location
  if (recordDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(recordDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(recordDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it's not anywhere, it isn't in a system header
  }
};

bool RecordDeclFormatter::isStruct() {
  return structOrUnion == STRUCT;
};

bool RecordDeclFormatter::isUnion() {
  return structOrUnion == UNION;
};

// tag_name is the name used as 'struct name' which C
// stores in a seperate symbol table. Structs are usually
// referred to by typedef provided, simpler names
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
        if (args.getSilent() == false) {
          errs() << "Warning: invalid struct field name " << identifier;
          errs() << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;
      }
      // Make sure that the field's identifier isn't too long for a fortran name
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);

      fieldsInFortran += "    " + tf.getFortranTypeASString(true) + " :: " + identifier + "\n";
    }
  }
  return fieldsInFortran;
}

// The procedure for any of the four sorts of structs/typedefs is fairly
// similar, but anonymous structs need to be handled specially. This 
// function puts together the name of the struct as well as the fields fetched
// from the getFortranFields() function above. All illegal names are prepended
// with h2m. Checks are made for duplicate names. Note that no option for a 
// bindname is allowed because BIND(C, name="") statements are illegail in
// a TYPE definition.
string RecordDeclFormatter::getFortranStructASString() {
  // initalize mode here
  setMode();
  string identifier = "";  // Holds the Fortran name for this structure.

  string rd_buffer;  // Holds the entire declaration.

  if (!isInSystemHeader) {  // Prevents system headers from leaking in to the file
    string fieldsInFortran = getFortranFields();
    if (fieldsInFortran.empty()) {
      rd_buffer = "! struct without fields may cause warnings\n";
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Warning: struct without fields may cause warnings: \n";
        LineError(sloc);
      }
    }

    if (mode == ID_ONLY) {
      identifier = recordDecl->getNameAsString();
            if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration by stepping through and appending ! before newlines
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n";
        temp_buf += fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      // Declare the structure in Fortran. The bindname may be empty.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == TAG_ONLY) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration by stepping through and appending ! before newlines
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n";
        temp_buf += fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ID_TAG) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration so that it is present in the output file
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n";
        temp_buf += fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      // Assemble the strucutre in Fortran. Note that bindname may be empty.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";    
    } else if (mode == TYPEDEF) {
      identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid typedef name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);


      // Check to see whether we have declared something with this identifier before.
      // Skip this duplicate declaration if necessary.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
        // Comment out the declaration so that it is present in the output file
        rd_buffer = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
        string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n";
        temp_buf += fieldsInFortran + "END TYPE " + identifier +"\n";
        std::istringstream in(temp_buf);
        for (std::string line; std::getline(in, line);) {
          rd_buffer += "! " + line + "\n";
        }
        return rd_buffer;
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ANONYMOUS) {  // No bindname options are specified for anon structs.
      // Note that no length checking goes on here because there's no need. 
      // This will all be commented out anyway.
      identifier = recordDecl->getTypeForDecl ()->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // Erase past all the spaces so that only the name remains (ie get rid of the "struct" part)
      size_t found = identifier.find_first_of(" ");
      while (found!=string::npos) {
        identifier.erase(0, found + 1);
        found=identifier.find_first_of(" ");
      }
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // We have previously seen a declaration with this name. This shouldn't be possible, so don't 
      // worry about commenting it out. It's commented out anyway.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          LineError(sloc);
        }
      }

      rd_buffer += "! ANONYMOUS struct may or may not have a declared name\n";
      string temp_buf = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE " + identifier +"\n";
      // Comment out the concents of the anonymous struct. There is no good way to guess at a name for it.
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        rd_buffer += "! " + line + "\n";
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line in anonymous struct" << line << " commented out \n";
          LineError(sloc);
        }
      }
    }
  }
  return rd_buffer;    
};

// Determines what sort of struct we are dealing with. The differences
// are subtle. If you need to understand what these modes are, you will
// have to play around with some structs. 
void RecordDeclFormatter::setMode() {
  // int ANONYMOUS = 0;
  // int ID_ONLY = 1;
  // int TAG_ONLY = 2;
  // int ID_TAG = 3;

  if (!(recordDecl->getNameAsString()).empty() && !tag_name.empty()) {
    mode = ID_TAG;
  } else if (!(recordDecl->getNameAsString()).empty() && tag_name.empty()) {
    mode = ID_ONLY;
  } else if ((recordDecl->getNameAsString()).empty() && !tag_name.empty()) {
    mode = TAG_ONLY;
  } else if ((recordDecl->getNameAsString()).empty() && tag_name.empty()) {
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
  // Because sloc is checked for validity prior to use, this should be a fine way to deal with
  // invalid locations
  if (funcDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(funcDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(funcDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
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
    // Check for a valid name length for the dummy variable.
    CheckLength(pname, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    
    CToFTypeFormatter tf((*it)->getOriginalType(), funcDecl->getASTContext(), sloc, args);

    // Array arguments must be handled diferently. They need the DIMENSION attribute.
    if (tf.isArrayType() == true) {
      paramsDecl += "    " + tf.getFortranArrayArgASString(pname) + "\n";
    } else {
      // in some cases parameter doesn't have a name in C, but must have one by the time we get here.
      paramsDecl += "    " + tf.getFortranTypeASString(true) + ", value" + " :: " + pname + "\n";
      // need to handle the attribute later - Michelle doesn't know what this (original) commment means 
    }
    // Similarly, check the length of the declaration line to make sure it is valid Fortran.
    // Note that the + 1 in length is to account for the newline character.
    CheckLength(pname, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
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
      string pname = (*it)->getNameAsString();
      if (pname.empty()) {
        pname = "arg_" + to_string(index);
      }
      if (pname.front() == '_') {  // Illegal character. Append a prefix.
        string old_pname = pname;
        pname = "h2m" + pname;
        if (args.getSilent() == false) {
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
        if (args.getSilent() == false) {
          errs() << "Warning: Illegal parameter name " << old_pname << " renamed ";
          errs() << pname << "\n";
          LineError(sloc);
        }
      }
      paramsNames += ", " + pname; 
    }
    index++;
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
  string fortranFunctDecl;
  if (!isInSystemHeader && argLocValid()) {
    string funcType;
    string paramsString = getParamsTypesASString();
    string imports;
    string bindname;  // Used to link to a C function with a different name later.
    if (!paramsString.empty()) {
      // CheckLength just returns the same string, but it will make sure the line is not too
      // long for Fortran and it will warn if needed and not silenced.
      imports = CheckLength("    USE iso_c_binding, only: " + getParamsTypesASString() + "\n",
          CToFTypeFormatter::line_max, args.getSilent(), sloc);
    } else {
      imports = "    USE iso_c_binding\n";
    }
    imports +="    import\n";
    
    // check if the return type is void or not
    // A void type means we create a subroutine. Otherwise a function is written.
    if (returnQType.getTypePtr()->isVoidType()) {
      funcType = "SUBROUTINE";
    } else {
      CToFTypeFormatter tf(returnQType, funcDecl->getASTContext(), sloc, args);
      funcType = tf.getFortranTypeASString(true) + " FUNCTION";
    }
    string funcname = funcDecl->getNameAsString();
    if (funcname.front() == '_') {  // We have an illegal character in the identifier
      if (args.getSilent() == false) {
        errs() << "Warning: invalid function name " << funcname << " renamed h2m" << funcname << "\n";
        LineError(sloc);
      }
      // If necessary, prepare a bind name to properly link to the C function
      if (args.getAutobind() == true) {
        // This is the proper syntax to bind to a C variable: BIND(C, name="cname")
        bindname = " , name =\"" + funcname + "\"";
      }
      funcname = "h2m" + funcname;  // Prepend h2m to fix the problem
    }
    // Check to make sure the function's name isn't too long. Warn if necessary.
    CheckLength(funcname, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check to make sure this declaration line isn't too long. It well might be.
    // bindname may be empty or may contain a C function to link to.
    fortranFunctDecl = CheckLength(funcType + " " + funcname + "(" + getParamsNamesASString() + 
        ")" + " BIND(C" + bindname + ")\n", CToFTypeFormatter::line_max, args.getSilent(), sloc);
    
    fortranFunctDecl += imports;
    fortranFunctDecl += getParamsDeclASString();
    // preserve the function body as comment
    if (funcDecl->hasBody()) {
      Stmt *stmt = funcDecl->getBody();
      clang::SourceManager &sm = rewriter.getSourceMgr();
      // comment out the entire function {!body...}
      string bodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(stmt->getSourceRange()),
          sm, LangOptions(), 0);
      string commentedBody;
      std::istringstream in(bodyText);
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: line " << line << " commented out \n";
          LineError(sloc);
        }
        commentedBody += "! " + line + "\n";
      }
      fortranFunctDecl += commentedBody;

    }
    if (returnQType.getTypePtr()->isVoidType()) {
      fortranFunctDecl += "END SUBROUTINE " + funcname + "\n\n";   
    } else {
      fortranFunctDecl += "END FUNCTION " + funcname + "\n\n";
    }
   
    // The guard function checks for duplicate identifiers. This might 
    // happen because C is case sensitive. It shouldn't happen often, but if
    // it does, the duplicate declaration needs to be commented out.
    if (RecordDeclFormatter::StructAndTypedefGuard(funcname) == false) { 
      if (args.getSilent() == false) {
        errs() << "Warning: duplicate declaration of " << funcname << ", FUNCTION, skipped.\n";
        LineError(sloc);
      }
      string temp_buf = fortranFunctDecl;
      fortranFunctDecl = "\n! Duplicate declaration of " + funcname + ", FUNCTION, skipped.\n";
      std::istringstream in(temp_buf);
      // Loops through the buffer like a line-buffered stream and comments it out
      for (std::string line; std::getline(in, line);) {
        fortranFunctDecl += "! " + line + "\n";
      }
    }
    
  }

  return fortranFunctDecl;
};

