// The following file contains the source code for the VarDeclFormatter
// for the h2m translator. This includes handling of regular variable
// declarations as well as structs and arrays.

#include "h2m.h"

// -----------initializer VarDeclFormatter--------------------
VarDeclFormatter::VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  varDecl = v;
  Okay = true;
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
        // A Structured type requires a special handling function.
        // Which occurs elsewhere (special call from getVarDeclAsString).
        // This should never be called but is here for safety.
    } else if (varDecl->getType().getTypePtr()->isCharType()) {
        // single CHAR
      char character = varDecl->evaluateValue()->getInt().getExtValue ();
      string cString;
      cString += character;
      valString = "\'" + cString + "\'";
    } else if (varDecl->getType().getTypePtr()->isIntegerType()) {
        // INT
      int intValue = varDecl->evaluateValue ()->getInt().getExtValue();
      valString = to_string(intValue);
    } else if (varDecl->getType().getTypePtr()->isRealType()) {
        // REAL
      valString = varDecl->evaluateValue()->getAsString(varDecl->getASTContext(), varDecl->getType());
    } else if (varDecl->getType().getTypePtr()->isComplexType()) {
        // COMPLEX
      APValue *apVal = varDecl->evaluateValue();
      if (apVal->isComplexFloat()) {
        float real = apVal->getComplexFloatReal().convertToFloat ();
        float imag = apVal->getComplexFloatImag().convertToFloat ();
        valString = "(" + to_string(real) + "," + to_string(imag) +")";
      } else if (apVal->isComplexInt()) {
        int real = apVal->getComplexIntReal().getExtValue();
        int imag = apVal->getComplexIntImag().getExtValue();
        valString = "(" + to_string(real) + "," + to_string(imag) +")";
      } 
    } else if (varDecl->getType().getTypePtr()->isPointerType()) {
      // POINTER 
      QualType pointerType = varDecl->getType();
      QualType pointeeType = pointerType.getTypePtr()->getPointeeType();
      if (pointeeType.getTypePtr()->isCharType()) {
        // string literal
        Expr *exp = varDecl->getInit();
        if (isa<ImplicitCastExpr>(exp)) {
          ImplicitCastExpr *ice = cast<ImplicitCastExpr> (exp);
          Expr *subExpr = ice->getSubExpr();
          if (isa<clang::StringLiteral>(subExpr)) {
            clang::StringLiteral *sl = cast<clang::StringLiteral>(subExpr);
            string str = sl->getString();
            valString = "\"" + str + "\"";
          }
        }

      } else {  // We don't recognize and can't handle this declaration.
        valString = "!" + varDecl->evaluateValue()->getAsString(varDecl->getASTContext(), varDecl->getType());
        if (args.getSilent() == false && args.getQuiet() == false) {
          errs() << "Variable declaration initialization commented out:\n";
          errs() << valString << "\n";
          CToFTypeFormatter::LineError(sloc); 
        }
      }
    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
      // ARRAY --- won't be used here bc it's handled by getFortranArrayDeclASString()
      // I'm not sure why this code is here at all in that case -Michelle
      Expr *exp = varDecl->getInit();
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc(),
          varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
      // comment out arrayText
      std::istringstream in(arrayText);
      for (std::string line; std::getline(in, line);) {
        if (args.getQuiet() == false && args.getSilent() == false) {
          errs() << "Warning: array contents " << line << " commented out \n";
          CToFTypeFormatter::LineError(sloc);
        }
        valString += "! " + line + "\n";
      }
    } else {
      valString = "!" + varDecl->evaluateValue()->getAsString(varDecl->getASTContext(),
          varDecl->getType());
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Variable declaration initialization commented out:\n" << 
            valString << " \n";
        CToFTypeFormatter::LineError(sloc);
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
// The "shapes" are the array dimensions (ie (2, 4, 2)). Initially, evaluatable
// should be set to false. The function passes informaiton back through it which
// essentially informs of success or failure. Array shapes will be passed back
// with an extra ", " either leading (if reversed array dimensions is true) or
// trailing (if array dimensions are not being reversed.)
void VarDeclFormatter::getFortranArrayEleASString(InitListExpr *ile, string &arrayValues,
    string &arrayShapes, bool &evaluatable, bool firstEle, bool is_char) {
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
  for (auto it = innerElements.begin(); it != innerElements.end(); it++) {
    Expr *innerelement = (*it);
    if (innerelement->isEvaluatable(varDecl->getASTContext())) {
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
  
};

// This will attempt to boil any Expr down into a corresponding
// Fortran string, but was specifically designed to be a recursive
// helper for fetching structures embeded in structure definitions.
// Note this function ONLY uses an expression, no information about the
// VarDecl is requested, so recursive calls are acceptable. When a fatal
// error occurs, success is set to "false" to let the caller know it should
// comment out the declaration.
string VarDeclFormatter::getFortranStructFieldsASString(Expr *exp, bool &success) {
  string structDecl = "";  // This will eventually hold all the fields.

  // The nullptr check is for safety.
  if (exp == nullptr) {
    success = false;
    // This is a serious enough problem taht it should always be reported.
    errs() << "Error: nullpointer in place of expression.\n";
    return "Internal error: nullpointer in place of expression.";
  }
  if (isa<InitListExpr>(exp)) {  // For a struct decl this should be true
    InitListExpr *ile = cast<InitListExpr>(exp);  // Obtain the list of values
    ArrayRef<Expr *> elements = ile->inits();

    // Set up a loop to iterate through all the expressions
    // which are part of the initialization of the structure.
    for (auto it = elements.begin(); it != elements.end(); it++) {
      Expr *element = (*it);
      if (element == nullptr) {  // Check for safety.
        success = false;
        // This is serious enough that it shoudl always be reported.
        errs() << "Error: nullpointer in place of InitListExpression element.\n";
        return "Internal error: nullpointer in place of InitListExpression element.";
      }
      string eleVal;  // This will hold the value to add to the declaration.
      QualType e_qualType = element->getType();
   
      if (element->isEvaluatable(varDecl->getASTContext())) {
        // We can, in some weird way, fold this down to a constant.
        clang::Expr::EvalResult r;  // Holds the result of the evaulation
        element->EvaluateAsRValue(r, varDecl->getASTContext());
        eleVal = r.Val.getAsString(varDecl->getASTContext(), e_qualType); 
          
        // Char types need to be handled specially so as not to be translated
        // into integers. Fortran does not like CHARACTER a = 97.
        if (e_qualType.getTypePtr()->isCharType() == true) {
            // Transfer the evaluated string to integer
            int temp_val = std::stoi(eleVal);
            char temp_char = static_cast<char>(temp_val);
            eleVal = "'";
            eleVal += temp_char;
            eleVal += "'";
                      
        // If the field is a pointer, determine if it is a string literal and
        // handle that, otherwise set it to a void pointer because what else
        // can we possibly do?
        } else if (e_qualType.getTypePtr()->isPointerType() == true) {
          QualType pointeeType = e_qualType.getTypePtr()->getPointeeType();

          // We've found a string literal.
          if (pointeeType.getTypePtr()->isCharType()) {
            if (isa<ImplicitCastExpr>(exp)) {
              ImplicitCastExpr *ice = cast<ImplicitCastExpr> (exp);
              Expr *subExpr = ice->getSubExpr();
              // Casts, if possible, the string literal into actual string-y form
              if (isa<clang::StringLiteral>(subExpr)) {
                clang::StringLiteral *sl = cast<clang::StringLiteral>(subExpr);
                string str = sl->getString();
                eleVal = "\"" + str + "\"" + ", ";
              }
            }
          // Other kinds of pointers are set to the appropriate null values because
          // it is not really feasible to create a proper Fortran translation.
          // Leave a message about this in the declaration.
          } else if (e_qualType.getTypePtr()->isFunctionPointerType()) {
            structDecl += "& ! Initial pointer value: " + eleVal + " set to C_NULL_FUNPTR\n";
            eleVal = "C_NULL_FUNPTR";
            if (args.getSilent() == false) {
              errs() << "Warning: pointer value " << eleVal << " set to C_NULL_FUNPTR\n";
              CToFTypeFormatter::LineError(sloc);
            }
          } else {
            structDecl += "& ! Initial pointer value: " + eleVal + " set to C_NULL_PTR\n";
            eleVal = "C_NULL_PTR";
            if (args.getSilent() == false) {
              errs() << "Warning: pointer value " << eleVal << " set to C_NULL_PTR\n";
              CToFTypeFormatter::LineError(sloc);
            }
          }
        // We have found an array as a subtype. Currently this only knows
        // how to handle string literals, but only string literals should
        // actually be evaluatable
        } else if (e_qualType.getTypePtr()->isArrayType() == true) {
          bool isChar = varDecl->getASTContext().getBaseElementType(e_qualType).getTypePtr()->isCharType();
          // This implies a string literal
          if (eleVal.front() == '&' && isChar == true) {
            // Erase up to the beginning of the " symbol.
            while (eleVal.front() != '"') {
              eleVal.erase(eleVal.begin(), eleVal.begin() + 1);
            }
          } else {  // Not a string literal... so we don't know what it is.
            // We warn, and arrange for the declaration to be commented out.
            eleVal = eleVal + " & ! Confusing array not translated.\n";
            if (args.getSilent() == false) {
              errs() << "Confusing sub-array initialization, " << eleVal << " not translated\n";
              CToFTypeFormatter::LineError(sloc);
            }
            success = false;
          }
        }
      } else {   // The element is NOT evaluatable.
        // If this is an array, try to make use of the helper function to 
        // parse it down into the base components.
        if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isArrayType() == true) {
          string values = "";
          string shapes = "";
          // This will hold the success value of array evaluation.
          bool array_success = false;
          if (isa<InitListExpr>(element)) {
            InitListExpr *in_list_exp = cast<InitListExpr>(element);
            // Determine whether this is a char array, needing special
            // evaluation, or not (do we need to cast an int to char?)
            bool isChar = varDecl->getASTContext().getBaseElementType(e_qualType).getTypePtr()->isCharType();
            // Call a helper to get the array in string form.
            getFortranArrayEleASString(in_list_exp, values, shapes, array_success,
              true, isChar);
          } else {
            eleVal = "UntranslatableArray ! ";
            success = false;
            // Get the array text we can't handle and put it in a comment.
            string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
                element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
            eleVal += value;
          }
          // If the array was succesfully evaluated, put together the translation
          if (array_success == true) {
            // Remove the extra ", " appended on to shapes by the helper from
            // the appropriate end. It will be at the back for the inverted array,
            // and at the front for the normal array dimensions.
            if (args.getArrayTranspose() == true) {
              shapes.erase(shapes.end() - 2, shapes.end());
            } else {
              shapes.erase(shapes.begin(), shapes.begin() + 2);
            }
            // A ", " is added by the helper function so one isn't appended here.
            eleVal = "RESHAPE((/" + values + "/), (/" + shapes + "/))";
          } else {
            eleVal = "UntranslatableArray ! ";
            success = false;
            string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
                element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
            eleVal += value;
         }
          // Make a recursive call to fill in the sub-structure.
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isStructureType() == true) {
          eleVal = e_qualType.getAsString() + "(";
          eleVal += getFortranStructFieldsASString(element, success) + ")";
          // If a pointer or function pointer is found, set it to 
          // the appropriate corresponding Fortran NULL value. Warn
          // if requested.
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isFunctionPointerType()) {
          eleVal = "C_NULL_FUNPTR";
          string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
              element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
          eleVal = "& ! Function pointer " + value + " set to C_NULL_FUNPTR\n" + eleVal;
          if (args.getSilent() == false) {
            errs() << "Warning: pointer value " << value << " set to C_NULL_FUNPTR\n";
            CToFTypeFormatter::LineError(sloc);
          }
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isPointerType() == true) {
          eleVal = "C_NULL_PTR";      
          string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
              element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
          eleVal = "& ! Pointer " + value + " set to C_NULL_PTR\n" + eleVal;
          if (args.getSilent() == false) {
            errs() << "Warning: pointer value " << value << " set to C_NULL_PTR\n";
            CToFTypeFormatter::LineError(sloc);
          }
        } else {  // We have no idea what this is or how to translate it. Warn and comment out.
          string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
              element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
          eleVal = "untranslatable component: " + value;
          if (args.getSilent() == false) {
            errs() << "Warning: unknown component not translated: " + value + ".\n";
            CToFTypeFormatter::LineError(sloc);
          }
          success = false;
        }
      }
    // Paste the generated, evaluated string value into place
    structDecl += eleVal + ", "; 
    }
    // Remove the extra ", " from the last pass and close the declaration.
    structDecl.erase(structDecl.end() - 2, structDecl.end());
  }
  return structDecl;
}

// This function fetches the declaration of the struct and its 
// inner fields only. The name checks occur elsewhere. This is
// just a helper for getting the fields. The boolean value is
// to determine whether or not this is a duplication of some
// other identifier and should thus be commented out.
// The form returned is "struct_name = (init1, init2...)"
string VarDeclFormatter::getFortranStructDeclASString(string struct_name, bool &success) {
  string structDecl = "";
  // This prevents system headers from leaking into the translation
  // This also makes sure we have an initialized structured type.
  if (varDecl->getType().getTypePtr()->isStructureType() && !isInSystemHeader &&
      varDecl->hasInit()) {  
    
    Expr *exp = varDecl->getInit();
    structDecl += " = " + struct_name + "(";
    // Make a call to the helper (which may make recursive calls)
    structDecl += getFortranStructFieldsASString(exp, success);
    structDecl += ")";
  }

  return structDecl;
}

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
    CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, 
        args.getSilent(), sloc);

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
        CToFTypeFormatter::LineError(sloc);
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
        CToFTypeFormatter::LineError(sloc);
      }
      arrayDecl = "! Skipping duplicate declaration of " + identifier + "\n!";
    }

    if (!varDecl->hasInit()) {
      // only declared, no initialization of the array takes place
      // Note that the bindname will be empty unless certain options are in effect and
      // the function's actual name is illegal.
      bool problem = false;
      arrayDecl += tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname + ") :: ";
      if (problem == true) {  // Set the object's problem flag
        Okay = false;
      }
      arrayDecl += tf.getFortranIdASString(identifier) + "\n";
    } else {
      // The array is declared and initialized. We must tranlate the initialization.
      const ArrayType *at = varDecl->getType().getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType();
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
        bool problem = false;
        arrayDecl += tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            tf.getFortranIdASString(identifier) + " = " + arrayText + "\n";
        if (problem == true) {  // Set the object's problem flag
          Okay = false;
        }
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
                QualType e_qualType = element->getType();
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
                } else {  // Otherwise append the element
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
                CToFTypeFormatter::LineError(sloc);
              }
            }
          } else {
            // The array is evaluatable and has been evaluated already. We assemble the
            //  declaration. INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /),
            //  (/2, 3/)). bindname may be empty.
            bool problem;
            arrayDecl += tf.getFortranTypeASString(true, problem)+", BIND(C" + bindname +  ") :: "+
                identifier +"("+arrayShapes_fin+")";
            if (problem == true) {  // Set the object's problem flag
              Okay = false;
            }
            arrayDecl += " = RESHAPE((/"+arrayValues+"/), (/"+arrayShapes_fin+"/))\n";
          }
        }
      }     
    }
  }
  // Check for lines which exceed the Fortran maximum. The helper will warn
  // if they are found and Silent is false. The +1 is because of the newline character
  // which doesn't count towards line length.
  CToFTypeFormatter::CheckLength(arrayDecl, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);

  return arrayDecl;
};



// Hanldes various variable declarations and returns them as fortran strings. Helper
// functions are called for arrays which are difficult to format. The type is 
// fetched with the helper function getFortranTypeASString. Any illegal identifiers (ie _thing)
// are prepended with "h2m" to become legal fortran identifiers.
string VarDeclFormatter::getFortranVarDeclASString() {
  string vd_buffer = "";
  string identifier = "";   // Will eventually hold the variable's name
  string bindname = "";  // May eventually hold a value to bind to (BIND (C, name ="...")
  bool struct_error = false;  // Flag for a special error during struct translation
  
  if (!isInSystemHeader) {  // This appears to protect local headers from having system
   // headers leak into the definitions
    // This is a declaration of a TYPE(stuctured_type) variable
    if (varDecl->getType().getTypePtr()->isStructureType()) {
      RecordDecl *rd = varDecl->getType().getTypePtr()->getAsStructureType()->getDecl();
      // Create a structure which is initialized in the module file.
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());

      if (identifier.front() == '_') {
         if (args.getSilent() == false) {
            errs() << "Warning: fortran names may not begin with an underscore. ";
            errs() << identifier << " renamed h2m" << identifier << "\n";
            CToFTypeFormatter::LineError(sloc);
         }
         if (args.getAutobind() == true) {  // Set up the bind phrase if requested.
           // The proper syntax is BIND(C, name="cname").
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      }

      // The following are checks for potentially illegal characters which might be
      // at the begining of the anonymous types. These types must be commented out.
      // It also looks for the "anonymous at" qualifier which might be present if 
      // the other tell-tale signs are not.
      bool problem;
      string f_type = tf.getFortranTypeASString(false, problem);
      if (problem == true) {  // Set the object's error flag for unrecognized types
        Okay = false;
      }
      if (f_type.front() == '/' || f_type.front() == '_' || f_type.front() == '\\' ||
          f_type.find("anonymous at") != string::npos) {
          // We don't need an ! here because everything will be commented out below.
          vd_buffer = " Anonymous struct declaration commented out. \n";
          struct_error = true;  // Set the special error flag.
      }
      // Assemble the variable declaration, including the bindname if it exists.
      vd_buffer += tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
      // This flag will tell us whether we had success fetching the struct declaration
      bool success = true;
      vd_buffer +=  ") :: " + identifier + getFortranStructDeclASString(f_type, success) + "\n";
      // This is another kind of error, the source of which we aren't sure of,
      // during an attempt to translate a structure. It will be checked for and
      // commented out later.
      if (success == false) {
        struct_error = true;
        // The ! will be added in during the global checks later.
        vd_buffer = "Unable to get initialization of structure\n" + vd_buffer;
      }
    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
      // Handle initialized numeric arrays specifically in the helper function.
      vd_buffer = getFortranArrayDeclASString();
      // Because most of the complicated array information is stored elsewhere,
      // checks for most errors are done in the helper. Only check for the 
      // invalid type problem here and comment out as necessary
      if (Okay == false && args.getDetectInvalid()) {
        std::istringstream in(vd_buffer);
        vd_buffer = "";
        if (args.getSilent() == false) {
          errs() << "Warning: illegal type in array.\n";
          CToFTypeFormatter::LineError(sloc);
        }
        for (std::string line; std::getline(in, line);) {
          vd_buffer += "! " + line + "\n";
        }
      }
      // The length and Okay checks everyone else does below would be 
      // repetitive, so skip them.
      return vd_buffer;
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
            CToFTypeFormatter::LineError(sloc);
         }
         if (args.getAutobind() == true) {  // Setup the autobinding buffer if requested
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      }
      // Determine the state of the declaration. Is there something declared? Is it commented out?
      // Create the declaration in correspondence with this. Add in the bindname, which may be empty
      if (value.empty()) {
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer +=  ") :: " + identifier + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
      } else if (value[0] == '!') {
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer +=  + ") :: " + identifier + " " + value + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
      } else {
        // A parameter may not have a bind(c) attribute.
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            identifier + " = " + value + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
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
            CToFTypeFormatter::LineError(sloc);
         }
         if (args.getAutobind() == true) {
           bindname = ", name=\"" + identifier + " \"";
         }
         identifier = "h2m" + identifier;
      } 
      // Determine the state of the declaration and assemble the appropriate Fortran equivalent.
      // Include the bindname, which may be empty.
      if (value.empty()) {
         bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer += ") :: " + identifier + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
      } else if (value[0] == '!') {
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer += ") :: " + identifier + " " + value + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
      } else {
        // A parameter may not have a bind(c) attribute
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            identifier + " = " + value + "\n";
        if (problem == true) {  // Set the object's error flag
          Okay = false;
        }
      }
    }

    // Check for repeats or for invalid types within the declaration.
    // If either is discovered, comment out the entire declaration.
    // Also check for the special structure error which might occur.
    bool not_duplicate = RecordDeclFormatter::StructAndTypedefGuard(identifier);
    if (not_duplicate == false || (Okay == false && args.getDetectInvalid() == true) ||
        struct_error == true) {
      string warning = "";  // This warning is printed
      string intext = "";  // The warning goes in the translated code.
      if (not_duplicate == false) {
        warning = "Variable declaration with name conflict, " + identifier + ", commented out.\n";
        intext = "! Variable with name conflict, " + identifier + ", commented out.\n";
      // When special errors occur, their warnings are already in the text
      // but we're not exactly sure what the error was.
      } else if (struct_error == true) {
        warning = "Error translating structure, " + identifier + "\n";
        // The intext warning was already added. Don't include another.
      } else {
        warning = "Variable with name conflict, " + identifier + ", commented out.\n";
        intext = "! Commenting out name conflict.\n";
      }
      // Warn about the mishap unless silenced.
      if (args.getSilent() == false) {
        errs() << warning;
        CToFTypeFormatter::LineError(sloc);
      }
      // Add the proper warning and comment out the buffer.
      std::istringstream in(vd_buffer);
      vd_buffer = intext;
      for (std::string line; std::getline(in, line);) {
        vd_buffer += "! " + line + "\n";
      }
    } else {  // We are not commenting things out, so we do length checks
      std::istringstream in(vd_buffer);
      // If we are not warning about line lengths, skip these checks,
      // otherwise perform them on every line (use silent==false for all.)
      if (args.getSilent() == false) {
        for (std::string line; std::getline(in, line);) {
          CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::line_max,
              false, sloc);
        }
      }
    }
    
    // Identifier may be initialized to "". This will cause no harm.
    CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);
  }
  return vd_buffer;
};

