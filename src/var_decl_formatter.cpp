// The following file contains the source code for the VarDeclFormatter
// for the h2m translator. This includes handling of regular variable
// declarations as well as structs and arrays.

#include "h2m.h"

// -----------initializer VarDeclFormatter--------------------
VarDeclFormatter::VarDeclFormatter(VarDecl *v, Rewriter &r, Arguments &arg) : 
    rewriter(r), args(arg) {
  varDecl = v;
  error_string = "";
  current_status = CToFTypeFormatter::OKAY;
  // Because sloc is checked for validity prior to use, this should handle invalid
  // locations. If it isn't initialized, it isn't valid according to the Clang
  //  check made in the helper.
  if (varDecl->getSourceRange().getBegin().isValid()) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        varDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(
        varDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header.
  }
};

// In the event that a variable declaration has an initial value, this function
// attempts to find that initialization value and return it as a string. It handles
// pointers, reals, complexes, characters, ints. Arrays are defined here but actually
// handled in their own function. Typedefs and structs are not handled here. They
// all require special consideration.
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
      char character = varDecl->evaluateValue()->getInt().getExtValue();
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
        // This parses out the string from a string literal
        // and then wraps it in Fortran syntax.
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
        valString = "!" + varDecl->evaluateValue()->getAsString(
            varDecl->getASTContext(), varDecl->getType());
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
    } else {  // Unknown variable declaration.
      current_status = CToFTypeFormatter::UNKNOWN_VAR;
      error_string = "Unknown variable declaration: " + valString;
    }
  }
  return valString;  // This is empty if we were in a system header.

};

// In order to handle initialization of arrays initialized on the spot,
// this function fetches the type and name of an array element through the 
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

      if (arrayValues.empty()) {  // Handle putting in the first element.
        arrayValues = eleVal;
      } else {
        arrayValues += ", " + eleVal;  // All others require a ", " before hand
      }
    // Recursively calls to find the smallest element of the array if
    // it is multidimensional (in which case elements are themselves
    // some form of array).
    } else if (isa<InitListExpr> (innerelement)) {
      InitListExpr *innerile = cast<InitListExpr>(innerelement);
      getFortranArrayEleASString(innerile, arrayValues, arrayShapes, evaluatable,
          (it == innerElements.begin()) && firstEle, is_char);
    } 
  }
  
};

// This will attempt to boil any Expr down into a corresponding
// Fortran string, but was specifically designed to be a recursive
// helper for fetching structures embeded in structure definitions.
// Note this function ONLY uses an expression, no information about the
// VarDecl is requested, so recursive calls are acceptable. It may,
// however, set the status and error string if needed.
string VarDeclFormatter::getFortranStructFieldsASString(Expr *exp) {
  string structDecl = "";  // This will eventually hold all the fields.

  // The nullptr check is for safety.
  if (exp == nullptr) {
    // This is a serious enough problem taht it should always be reported.
    current_status = CToFTypeFormatter::CRIT_ERROR;
    error_string = "Error: nullpointer in place of expression.";
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
        current_status = CToFTypeFormatter::CRIT_ERROR;
        error_string = "Error: nullpointer in place of expression.";
        // This is serious enough that it should always be reported.
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
            // Turn the integer back into a character
            eleVal = "'";
            eleVal += temp_char;
            eleVal += "'";
                      
        // If the field is a pointer, determine if it is a string literal and
        // handle that, otherwise set it to a void pointer because what else
        // can we possibly do? Fortran pointer syntax is too different.
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
                // Now we paste the string into fortran string syntax.
                eleVal = "\"" + str + "\"" + ", ";
              }
            }
          // Other kinds of pointers are set to the appropriate null values because
          // it is not really feasible to create a proper Fortran translation.
          // Leave a message about this in the declaration by linking multiple
          // lines with & and putting the comment before the declaration
          //  "& ! COMMENT \n DECL".
          } else if (e_qualType.getTypePtr()->isFunctionPointerType()) {
            structDecl += "& ! Initial pointer value: " + eleVal +
                " set to C_NULL_FUNPTR\n";
            if (args.getSilent() == false) {
              errs() << "Warning: pointer value " << eleVal << 
                  " set to C_NULL_FUNPTR\n";
              CToFTypeFormatter::LineError(sloc);
            }
            eleVal = "C_NULL_FUNPTR";
          } else {
            structDecl += "& ! Initial pointer value: " + eleVal + 
                " set to C_NULL_PTR\n";
            if (args.getSilent() == false) {
              errs() << "Warning: pointer value " << eleVal << 
                  " set to C_NULL_PTR\n";
              CToFTypeFormatter::LineError(sloc);
            }
            eleVal = "C_NULL_PTR";
          }
        // We have found an array as a subtype. Currently this only knows
        // how to handle string literals, but only string literals should
        // actually be evaluatable. All others will lead to recursive calls.
        } else if (e_qualType.getTypePtr()->isArrayType() == true) {
          bool isChar = varDecl->getASTContext().getBaseElementType(
              e_qualType).getTypePtr()->isCharType();
          // This implies a string literal has been found.
          if (eleVal.front() == '&' && isChar == true) {
            // Erase up to the beginning of the & symbol which may be present because 
            // C treats string literals as pointers to static memory.
            // By doing this you get a string in the form "string".
            while (eleVal.front() != '"') {
              eleVal.erase(eleVal.begin(), eleVal.begin() + 1);
            }
          } else {  // Not a string literal... so we don't know what it is.
            // We warn, and arrange for the declaration to be commented out.
            eleVal = eleVal + " & ! Confusing array not translated.\n";
            error_string = "Confusing array element.";
            current_status = CToFTypeFormatter::BAD_ARRAY; 
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
            // evaluation, or not (do we need to cast an int to char to
            // avoid saying "CHARACTER(C_CHAR) = RESHAPE((/97...")))?
            bool isChar = varDecl->getASTContext().getBaseElementType(
                e_qualType).getTypePtr()->isCharType();
            // Call a helper to get the array in string form. This function deals with all possible 
            // array nesting. The array_success flag will reflect the helper's status.
            getFortranArrayEleASString(in_list_exp, values, shapes, array_success,
              true, isChar);
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
          } else {  // The translation failed.
            eleVal = "UntranslatableArray:";
            current_status = CToFTypeFormatter::BAD_ARRAY;
            // Fetch the array text and return that.
            string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
                element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
            eleVal += value;
            error_string = value;
         }
        // Make a recursive call to fill in the sub-structure embeded in the main structure.
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isStructureType() == true) {
          // First, get the structure's name (hopefully) and append "(" to create "type_name(".
          eleVal = e_qualType.getAsString() + "(";
          eleVal += getFortranStructFieldsASString(element) + ")";
          // If a pointer or function pointer is found, set it to 
          // the appropriate corresponding Fortran NULL value. Warn
          // if requested.
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isFunctionPointerType()) {
          eleVal = "C_NULL_FUNPTR";
          // Fetch the initialization text for this function pointer.
          string value = Lexer::getSourceText(CharSourceRange::getTokenRange(element->getLocStart(),
              element->getLocEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
          eleVal = "& ! Function pointer " + value + " set to C_NULL_FUNPTR\n" + eleVal;
          if (args.getSilent() == false) {
            errs() << "Warning: pointer value " << value << " set to C_NULL_FUNPTR\n";
            CToFTypeFormatter::LineError(sloc);
          }
        } else if (e_qualType.getTypePtr()->getUnqualifiedDesugaredType()->isPointerType() == true) {
          eleVal = "C_NULL_PTR";      
          // Fetch the initialization for this pointer.
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
          eleVal = "UntranslatableComponent:" + value;
          error_string = "Untranslatable structure componenet: " + value;
          current_status = CToFTypeFormatter::BAD_STRUCT_TRANS;
        }
      }
    // Paste the generated, evaluated string value into place
    structDecl += eleVal + ", "; 
    }
    // Remove the extra ", " from the last pass.
    structDecl.erase(structDecl.end() - 2, structDecl.end());
  }
  return structDecl;
}

// This function fetches the declaration of the struct and its 
// inner fields only. The name checks occur elsewhere. The boolean value is
// to determine whether or not this is a duplication of some
// other identifier and should thus be commented out, or has some
// other problem such as an unrecognized field.
// The form returned is "struct_name = struct_type(init1, init2...)"
string VarDeclFormatter::getFortranStructDeclASString(string struct_name) {
  string structDecl = "";
  // This prevents system headers from leaking into the translation
  // This also makes sure we have an initialized structured type.
  if (varDecl->getType().getTypePtr()->isStructureType() && !isInSystemHeader &&
      varDecl->hasInit()) {  
    
    Expr *exp = varDecl->getInit();
    structDecl += " = " + struct_name + "(";
    // Make a call to the helper (which may make recursive calls).
    structDecl += getFortranStructFieldsASString(exp);
    // Close the declaration.
    structDecl += ")";
  }

  return structDecl;
}

// Much more complicated function used to generate an array declaration. 
// Syntax for C and Fortran arrays are completely different. The array 
// must be declared and initialization carried out if necessary.
// Special circumstances involving unevaluatable componenets, character
// arrays etc. are dealt with here and in the associated helper.
string VarDeclFormatter::getFortranArrayDeclASString() {
  string arrayDecl = "";
  // This keeps system header pieces from leaking into the translation
  if (varDecl->getType().getTypePtr()->isArrayType() && !isInSystemHeader) {
    CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
    
    // If asked to autobind, this string holds the identifier to bind to;
    // it is initialized as empty but may contain a name later.
    // The syntax used is name 'BIND(C, name="c_name")'.
    string bindname = "";
    string identifier = varDecl->getNameAsString();

    // This boolean decides whether or not there array dimensions
    // contain *, a variable size array, which is only legal in
    // Fortran in a function declaration.
    bool is_star = (tf.getFortranArrayDimsASString().find("*") !=
        std::string::npos);
    if (is_star == true) {  // There is a * in the array dimensiosn
      current_status = CToFTypeFormatter::BAD_STAR_ARRAY;
      error_string = identifier;
    }
    // Illegal underscore is found in the array declaration
    if (identifier.front() == '_') {
      // If necessary, prepare a bind name to properly link to the C function
      if (args.getAutobind() == true) {
        // This is the proper syntax to bind to a C variable: BIND(C, name="cname")
        bindname = ", name=\"" + identifier + "\"";
      }
      CToFTypeFormatter::PrependError(identifier, args, sloc);
      identifier = "h2m" + identifier;
    }

    if (!varDecl->hasInit()) {
      // The array is only declared, no initialization of the array takes place.
      // Note that the bindname will be empty unless certain options are in effect and
      // the function's actual name is illegal.
      bool problem = false;
      arrayDecl += tf.getFortranTypeASString(true, problem) + 
          ", public, BIND(C" + bindname + ") :: ";
      if (problem == true) {  // We have encountered an unrecognized type.
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = tf.getFortranTypeASString(true, problem);
      }
      arrayDecl += tf.getFortranIdASString(identifier) + "\n";
    } else {
      // The array is declared and initialized. We must tranlate the initialization.
      const ArrayType *at = varDecl->getType().getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType();
      Expr *exp = varDecl->getInit();
      // Whether this is a char array or not will have to be checked several times.
      // This checks whether or not the "innermost" type is a char type.
      bool isChar = varDecl->getASTContext().getBaseElementType(
          e_qualType).getTypePtr()->isCharType();
      // This fetches the actual text initialization, which may be 
      // a string literal or {'a', 'b'...}.
      string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(exp->getExprLoc(),
            varDecl->getSourceRange().getEnd()), rewriter.getSourceMgr(), LangOptions(), 0);
     // A char array might not be a string literal. A leading { character indicates this case.
     if (isChar == true && arrayText.front() != '{') {
        // handle stringliteral case.
        // A parameter may not have a bind(c) attribute
        bool problem = false;  // The helper will set this flag appropriately.
        arrayDecl += tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            tf.getFortranIdASString(identifier) + " = " + arrayText + "\n";
        if (problem == true) {  // We have encountered an illegal type.
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem);
        }
      } else {  // This is not a string literal but a standard C array.
        bool evaluatable = false;
        Expr *exp = varDecl->getInit();
        if (isa<InitListExpr> (exp)) {
          // initialize shape (dimensions) and values
          // format: INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /), (/2, 3/))
          string arrayValues;  // Initialization values
          string arrayShapes;  // Array dimensions

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
              getFortranArrayEleASString(innerIle, arrayValues, arrayShapes, 
                  evaluatable, it == elements.begin(), isChar);
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
                  // This is split into three lines to avoid some problems with overloaded string '+'.
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
            // get fortran equivalents. We comment out the declaration during error handling.
            string arrayText = Lexer::getSourceText(CharSourceRange::getTokenRange(
                varDecl->getSourceRange()), rewriter.getSourceMgr(), LangOptions(), 0);
              // We comment out arrayText using the string stream.
              current_status = CToFTypeFormatter::BAD_ARRAY;
              error_string = arrayText;
          } else {
            // The array is evaluatable and has been evaluated already. We assemble the
            //  declaration. INTEGER(C_INT) :: array(2,3) = RESHAPE((/ 1, 2, 3, 4, 5, 6 /),
            //  (/2, 3/)). The bindname may be empty.
            bool problem = false;  // The helper sends back this flag for an invalid type
            arrayDecl += tf.getFortranTypeASString(true, problem)+", BIND(C" + bindname
                 +  ") :: "+ identifier +"("+arrayShapes_fin+")";
            if (problem == true) {  // We have found a bad type.
              current_status = CToFTypeFormatter::BAD_TYPE;
              error_string = identifier + ", array definition.";
            }
            arrayDecl += " = RESHAPE((/"+arrayValues+"/), (/"+arrayShapes_fin+"/))\n";
          }
        }
      }     
    }
  }
  return arrayDecl;
};



// Handles various variable declarations and returns them as Fortran strings. Helper
// functions are called for arrays which are difficult to format. The type is 
// fetched with the helper function getFortranTypeASString. Any illegal identifiers
// (ie _thing) are prepended with "h2m" to become legal fortran identifiers.
string VarDeclFormatter::getFortranVarDeclASString() {
  string vd_buffer = "";
  string identifier = "";   // Will eventually hold the variable's name
  string bindname = "";  // May eventually hold a value to bind to (BIND (C, name ="...")
  bool struct_error = false;  // Flag for a special error during struct translation
  
  if (!isInSystemHeader) {  // This appears to protect local headers from having system
  // headers leak into the definitions.
    // This is a declaration of a TYPE(stuctured_type) variable
    if (varDecl->getType().getTypePtr()->isStructureType()) {
      RecordDecl *rd = varDecl->getType().getTypePtr()->getAsStructureType()->getDecl();
      // Create a structure which is initialized in the module file.
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());

      // Deal with an illegal identifier and a potential need for a bind name.
      if (identifier.front() == '_') {
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        if (args.getAutobind() == true) {  // Set up the bind phrase if requested.
          // The proper syntax is BIND(C, name="cname").
          bindname = ", name=\"" + identifier + "\"";
        }
        identifier = "h2m" + identifier;
      }

      // The following are checks for potentially illegal characters which might be
      // at the begining of the anonymous types. These types must be commented out.
      // It also looks for the "anonymous at" qualifier which might be present if 
      // the other tell-tale signs are not. NOTE: the helper function will usually
      // catch anonymous types, so these checks may be unnecessary now.
      bool problem;  // This will be set to true if the helper sees an illegal type.
      string f_type = tf.getFortranTypeASString(false, problem);
      if (problem == true) {  // Set the object's error flag for unrecognized types
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = f_type + ", in struct.";
      }
      if (f_type.front() == '/' || f_type.front() == '_' || f_type.front() == '\\' ||
          f_type.find("anonymous at") != string::npos) {
        // We don't need an ! here because everything will be commented out below.
        struct_error = true;  // Set the special error flag.
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = f_type + ", anonymous in struct.";
      }
      // Assemble the variable declaration, including the bindname if it exists.
      vd_buffer += tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
      // Get the struct translation.
      vd_buffer +=  ") :: " + identifier + getFortranStructDeclASString(f_type) + "\n";
    } else if (varDecl->getType().getTypePtr()->isArrayType()) {
      // Handle initialized numeric arrays specifically in the helper function.
      // We fetch the identifier here only to use it later to check for repeats
      // or names which are too long. The warnings are printed elsewhere.
      identifier = varDecl->getNameAsString();
      if (identifier[0] == '_') {
        identifier = "h2m" + identifier;
      }
      // This strips off potential size modifiers so we only get the name
      // of the array: n rather than n(4,3).
      identifier = identifier.substr(0, identifier.find_first_of("("));
      vd_buffer = getFortranArrayDeclASString();
    } else if (varDecl->getType().getTypePtr()->isPointerType() && 
      varDecl->getType().getTypePtr()->getPointeeType()->isCharType()) {
      // This is a string declaration
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType().getTypePtr()->getPointeeType(),
          varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      // Check for an illegal character at the string identifier's start.
      if (identifier.front() == '_') {
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        if (args.getAutobind() == true) {  // Setup the autobinding buffer if requested
          bindname = ", name=\"" + identifier + "\"";
        }
        identifier = "h2m" + identifier;
      }
      // Determine the state of the declaration. Is there something declared? Is it commented out?
      // Create the declaration in correspondence with this. Add in the bindname, which may be empty
      if (value.empty()) {
        bool problem = false;  // The helper sets this flag to show status.
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer +=  ") :: " + identifier + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      } else if (value[0] == '!') {
        bool problem = false;  // The helper will set this flag if it sees an illegal type.
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer +=  + ") :: " + identifier + " " + value + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      } else {
        // A parameter may not have a bind(c) attribute.
        bool problem = false;  // The helper will set this flag if it sees an illegal type.
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            identifier + " = " + value + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      }
      // If it is not a structure, pointer, or array, proceed with no special treatment.
    } else {
      string value = getInitValueASString();
      CToFTypeFormatter tf(varDecl->getType(), varDecl->getASTContext(), sloc, args);
      identifier = tf.getFortranIdASString(varDecl->getNameAsString());
      // Check for an illegal name.
      if (identifier.front() == '_') {
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        if (args.getAutobind() == true) {  // Set the BIND(C, name=..." to link to the c name
          bindname = ", name=\"" + identifier + "\"";
        }  
        identifier = "h2m" + identifier;
      } 
      // Determine the state of the declaration and assemble the appropriate Fortran equivalent.
      // Include the bindname, which may be empty.
      if (value.empty()) {  // This is an actual variable, uninitialized.
        bool problem = false;  // The helper sets this flag if it sees an illegal type.
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer += ") :: " + identifier + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      } else if (value[0] == '!') {  // This is left over from old logic but is her for safety.
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", public, BIND(C" + bindname;
        vd_buffer += ") :: " + identifier + " " + value + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      } else {  // This is an initialized variable to be treated as a parameter.
        // A parameter may not have a bind(c) attribute
        bool problem = false;
        vd_buffer = tf.getFortranTypeASString(true, problem) + ", parameter, public :: " +
            identifier + " = " + value + "\n";
        if (problem == true) {  // We've found an unrecognized type
          current_status = CToFTypeFormatter::BAD_TYPE;
          error_string = tf.getFortranTypeASString(true, problem) + ", in variable.";
        }
      }
    }

    // Check for a repeated identifier.
    bool not_duplicate = RecordDeclFormatter::StructAndTypedefGuard(identifier);
    if (not_duplicate == false) {  // This is a duplicate identifier
      current_status = CToFTypeFormatter::DUPLICATE;
      error_string = identifier;
    }
    // Check for an overly long identifier.
    if (identifier.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = identifier;
    }
    // Check for an excessively long line in the declaration by
    // looping through all the lines in a string stream. An empty
    // string will not be a problem.
    std::istringstream in(vd_buffer);
    for (std::string line; std::getline(in, line);) {
      // Trim the line to the point where a comment begins if it does.
      line = line.substr(0, line.find_first_of("!"));
      if (line.length() > CToFTypeFormatter::line_max) {
        current_status = CToFTypeFormatter::BAD_LINE_LENGTH;
        error_string = line + ", in variable declaration.";
      }
    }
  }  // End processing of non-system headers
  return vd_buffer;
};

