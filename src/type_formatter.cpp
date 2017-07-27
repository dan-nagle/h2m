// This file holds the code for the CToFTypeFormatter class
// for the h2m autofortran tool. 

#include "h2m.h"

// A helper function to be used to output error line information
// If the location is invalid, it returns a message about that.
void CToFTypeFormatter::CToFTypeFormatter::LineError(PresumedLoc sloc) {
  if (sloc.isValid()) {
    errs() << sloc.getFilename() << " Line " << sloc.getLine() << "\n";
  } else {
    errs() << "Invalid file location \n";
  }
}

// This little helper outputs an error relating to prepending h2m to the
// front of an identifier, if needed. It's nice to have a generic
// error message in one place.
void CToFTypeFormatter::PrependError(const string identifier, Arguments& args,
    PresumedLoc sloc) {
  if (args.getSilent() == false) {
    errs() << "Warning: Fortran identifiers may not begin with an underscore. " <<
        identifier << " renamed h2m" << identifier << "\n";
    LineError(sloc);
  }
}


// This complicated function determines from status and arguments what errors
// should be emitted and whether a buffer should be commented out after a 
// translation. The error string and a call to LineError will be emitted if
// the argument's value of quite and silent requires it. The error string should
// be the text of the problem. A description will be added in.
// The current_status is the status associated with the object that produced the error
// and translation string. sloc is the location in the source files where
// the translation began. args is the Arguments object associated with the 
// current tool run.
string CToFTypeFormatter::EmitTranslationAndErrors(status current_status, string
    error_string, string translation_string, PresumedLoc sloc, Arguments &args) {
  bool silent = args.getSilent();  // This is for ease of access.
  bool emit_errors = false;  // Boolean to decide whether to print errors.
  bool comment_out = false;  // Boolean to decide whether to comment out text.

  // From the status code, determine what kind of warnings to give
  // and whether to comment out the text.
  if (current_status == OKAY) {  // No problem. Send the string right back.
    return translation_string; 
  // Under certain options, we comment out function like macros.
  } else if (current_status == FUNC_MACRO) {
    if (args.getHideMacros() == false) {
      return translation_string;  // No problem. Send the string right back.
    }
    translation_string = "Found function like macro.\n" + translation_string;
    error_string = "Warning: function like macro commented out: " + error_string;
    emit_errors = !silent;  // If not silent, emit errors.
    comment_out = true; 
  // If there is an unknown type we may want to comment it out.
  } else if (current_status == BAD_TYPE) {
    emit_errors = !silent; 
    // There is an option to disable commenting out of this type
    // of problem. We check to see if that option has been invoked.
    comment_out = args.ShouldCommentOut(BAD_TYPE);
    translation_string = "Found illegal type.\n" + translation_string;
    error_string = "Warning: Unrecognized or illegal type found: " + error_string;
  // An anonymous type has been discovered.
  } else if (current_status == BAD_ANON) {
    error_string = "Warning: anonymous type found:" + error_string;
    translation_string = "Found anonymous type.\n" + translation_string;
    comment_out = args.ShouldCommentOut(BAD_ANON);
    emit_errors = !silent;
  // Deal with illegally long names and lines.
  } else if (current_status == BAD_LINE_LENGTH) {
    comment_out = args.ShouldCommentOut(BAD_LINE_LENGTH);
    emit_errors = !silent;
    error_string = "Warning: line exceeding length maximum found: " + error_string;
    translation_string = "Found excessively long line.\n" + translation_string;
  } else if (current_status == BAD_NAME_LENGTH) {
    comment_out = args.ShouldCommentOut(BAD_NAME_LENGTH);
    emit_errors = !silent;
    error_string = "Warning: name exceeding length maximum found: " + error_string;
    translation_string = "Found excessively long name.\n" + translation_string;
  // Something went wrong translating a structure initialization.
  } else if (current_status == BAD_STRUCT_TRANS) {
    comment_out = true;
    emit_errors = !silent;  
    error_string = "Warning: stucture translation failure: " + error_string;
    translation_string = "Found structure translation failure.\n" +
        translation_string;
  // This is an assumed size array in an illegal location.
  } else if (current_status == BAD_STAR_ARRAY) {
    comment_out = true;
    emit_errors = !silent;
    error_string = "Warning: bad use of variable size array: " + error_string;
    translation_string = "Found bad use of variable size array\n" +
        translation_string; 
  // An attempt to translate a variable has gone awry.
  } else if (current_status == UNKNOWN_VAR) {
    comment_out = true;
    emit_errors = !silent;
    error_string = "Warning: failed variable translation: " + error_string;
    translation_string = "Found bad variable translation.\n" + translation_string;
  // This is an internal error ie a nullpointer where one should not be.
  } else if (current_status == CRIT_ERROR) {
    comment_out = true;
    emit_errors = true;
    error_string = "Error during translation: " + error_string;
    translation_string = "Error during translation.\n" + translation_string;
  // This is a holdover from when h2m could not translate long or
  // unsigned macros. 
  } else if (current_status == U_OR_L_MACRO) {
    comment_out = true;
    emit_errors = !silent;
    error_string = "Warning: unsupported type in macro: " + error_string;
    translation_string = "Found unsupported macro type\n" + translation_string;
  // A duplicate identifier was discovered.
  } else if (current_status == DUPLICATE) {
    // There is an option to not comment out this problem. 
    // Check to see if it has been invoked.
    comment_out = args.ShouldCommentOut(DUPLICATE);
    emit_errors = !silent;
    error_string = "Warning: duplicate identifier detected: " + error_string;
    translation_string = "Found duplicate identifier.\n" + translation_string;
  } else if (current_status == BAD_ARRAY) {
    comment_out = true;
    emit_errors = !silent;
    error_string = "Warning: failed translation of array: " + error_string;
    translation_string = "Found failed array translation.\n" + translation_string;
  // An unrecognized macro type could not be translated.
  } else if (current_status == BAD_MACRO) {
    comment_out = true;
    emit_errors = !silent;
    error_string = "Warning: unrecognized macro type not translated: " + error_string;
    translation_string = "Found unrecognized macro.\n" + translation_string;
  } else {  // What sort of error is this?
    comment_out = true;
    emit_errors = !silent;
    error_string = "ERROR: unrecognized error code: " + error_string;
    translation_string = "Unknown error\n" + translation_string;
  } 

  // Emit the errors if requested. Add in a newline for readabiilty.
  if (emit_errors == true) {
    errs() << error_string << "\n";
    CToFTypeFormatter::LineError(sloc);
  }

  // Use a string stream to iterature through the lines of the declaration
  // and comment it all out if necessary.
  if (comment_out == true) {  // Comment out the declaration
    std::istringstream in(translation_string);
    translation_string = "";  // Zero out the string and put in commented text.
    for (std::string line; std::getline(in, line);) {
      translation_string += "! " + line + "\n";
    }
  // If we are not commenting out the whole buffer, we need to comment out
  // the first line in order to hide the error-explanation string which
  // was added above.
  } else {
    translation_string = "! " + translation_string;
  }
  return translation_string;
}

// -----------initializer RecordDeclFormatter--------------------
CToFTypeFormatter::CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc loc,
    Arguments &arg): ac(ac), args(arg) {
  c_qualType = qt;
  sloc = loc;
};

// Determines whether the qualified type offered is identical to that it is called on.
// Pointer types are only distinguished in terms of function vs data pointers. 
bool CToFTypeFormatter::isSameType(QualType qt2) {
  // for pointer type, only distinguish between the function pointer from other pointers
  if (c_qualType.getTypePtr()->isPointerType() && qt2.getTypePtr()->isPointerType()) {
    // True if both are function pointers
    if (c_qualType.getTypePtr()->isFunctionPointerType() && 
        qt2.getTypePtr()->isFunctionPointerType()) {
      return true;
    // True if both are not function pointers
    } else if ((!c_qualType.getTypePtr()->isFunctionPointerType()) &&
         (!qt2.getTypePtr()->isFunctionPointerType())) {
      return true;
    } else {
      return false;
    }
  } else {
    // No pointers involved. Use the overloaded operator ==
    return c_qualType == qt2;
  }
};

// Typically this returns the raw id, but in the case of an array, it must 
// determine an array suffix by calling a helper.
string CToFTypeFormatter::getFortranIdASString(string raw_id) {
  // Determine if it needs to be substituted out because
  // it is an array and needs size information.
  if (c_qualType.getTypePtr()->isArrayType()) {
    // The helper fetches the dimensions in the form "x, y, z"
    raw_id += "(" + getFortranArrayDimsASString() + ")";
  }
  return raw_id;
};

// This function is for use with arrays which are not initialized (usually).
// This function will return the raw dimensions of an array as a comma separated
// list "1, 2, 3". If requested on the command line, the dimensions will be reversed.
// In the case that the array does not have constant dimensions, proper syntax
// for an assumed shape array will be employed.
string  CToFTypeFormatter::getFortranArrayDimsASString() {
  string dim_str = "";  // String of dimensions to return.

  QualType element_type = c_qualType;  // Element type of the array for looping.
  const Type *the_type_ptr = element_type.getTypePtr();
  // Loop through the array's dimensions, pulling off layers by fetching
  // the element types and getting their array information. The nullptr
  // check is just for safety.
  while (the_type_ptr != nullptr && the_type_ptr->isArrayType() == true) {
    // A constant array type has specified dimensions.
    if (the_type_ptr->isConstantArrayType()) {
      const ConstantArrayType *cat = ac.getAsConstantArrayType(element_type);
      // Note that the getSize() function returns an arbitrary precision integer
      // and a toString method needs the radix (10) and signed status (true).
      // The if statement decides where to put dimensions to properly reverse
      // them if requested.
      if (args.getArrayTranspose() == true) {
        dim_str = cat->getSize().toString(10, true) + ", " +  dim_str;
      } else {
        dim_str += cat->getSize().toString(10, true) + ", ";
      }
      element_type = cat->getElementType();  // Iterate to the next dimension.
      the_type_ptr = element_type.getTypePtr();
    // A variable type array has an expression making up its size ie "arr[x + 15]"
    // or is of the form x[*] (which is annoying and requires special handling).
    } else if (the_type_ptr->isVariableArrayType() == true) {
      const VariableArrayType *vat = ac.getAsVariableArrayType(element_type); 
      Expr* size_exp = vat->getSizeExpr();  // Get information about the array size expression.
       
      clang::Expr::EvalResult eval_result;
      // Check whether or not crazy, non-standard techniques can evaluate this value.
      // If true, the result is returned in 'eval_result'
      if (size_exp != nullptr && size_exp->EvaluateAsRValue(eval_result, ac) == true) {
        string eval_str = eval_result.Val.getAsString(ac, vat->getElementType()); 
        if (args.getArrayTranspose() == true) {
          dim_str = eval_str + ", " + dim_str;
        } else {
          dim_str += eval_str + ", ";
        }
      // If an array is declared as * size, we can just add "*, ". There are three
      // possible array size modifiers, Normal, Star, and Static (func decls only)
      } else if (vat->getSizeModifier() == ArrayType::ArraySizeModifier::Star) {
        return "*";  // The approximation is as an assumed size array.
        // Fortran does not allow syntax such as int(*,*,*) or double(5,*).
      } else {  // We cannot evaluate the expression. We fetch the source text.
        string expr_text = Lexer::getSourceText(CharSourceRange::getTokenRange(
            vat->getBracketsRange()), ac.getSourceManager(), LangOptions(), 0);
        // This will give us the square brackets, so remove those from the ends
        // because there should be no square brackets in Fortran array dimensions.
        // This length test is to prevent an exception if something weird
        // has happened during the evaluation and the expressions is too small.
        if (expr_text.length() > 2) {
          expr_text.erase(expr_text.begin(), expr_text.begin() + 1);
          expr_text.erase(expr_text.end()-1, expr_text.end());
        }

        // Put our possibly illegal expression into place with the other dimensions.
        if (args.getArrayTranspose() == true) {
          dim_str = expr_text + ", " + dim_str;
        } else {
          dim_str += expr_text + ", ";
        }
        // This is likely a serious issue. It may prevent compilation. There is
        // no guarantee that this expression is evaluatable in Fortran.
        if (args.getSilent() == false) { 
          errs() << "Warning: unevaluatable array dimensions: " << expr_text << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
      }
    
      // Get information about the next dimension of the array and repeat.
      element_type = vat->getElementType();
      the_type_ptr = element_type.getTypePtr();
    // An incomplete type array has an unspecified size ie "arr[]"
    } else if (the_type_ptr->isIncompleteArrayType()) {
      return "*";  // Again, the approximate equivalent is as an 
      // assumed shape array.
    }
  }
  // There is one other possibility: DependentSizedArrayType, but this is a C++
  // construct that h2m does not support (DSAT's are template based arrays).

  // We erase the extra space and comma from the last loop iteration, or the
  // first iteration (if bounds were reversed). Regardless, erase them.
  dim_str.erase(dim_str.end() - 2, dim_str.end());
  return dim_str;
}

// This function will create a specification statement for a Fortran array argument
// as might appear in a function or subroutine prototype, (ie for "int thing (int x[5])"
// it returns "INTEGER(C_INT), DIMENSION(5) :: x").
string CToFTypeFormatter::getFortranArrayArgASString(string dummy_name) {
  bool problem = false;  // Flag that determines helper's success
  string arg_buff = getFortranTypeASString(true, problem) + ", DIMENSION(";
  // We ignore the result of problem becuase it will be checked by
  // the calling function, too, and woudl be inconvenient to check here.
  arg_buff += getFortranArrayDimsASString() + ") :: ";
  arg_buff += dummy_name;
  return arg_buff;
}

// This function will determine whether or not the type given is an array
// of any form. This is necessary to decide how to format function arguments.
bool CToFTypeFormatter::isArrayType() {
  if (c_qualType.getTypePtr() != nullptr && c_qualType.getTypePtr()->isArrayType()) {
    return true;
  }
  return false;
}

// The type in C is found in the c_qualType variable. The type is 
// determined over a series of if statements and a suitable Fortran 
// equivalent is returned as a string. The boolean typeWrapper dictates
// whether or not the return should be of the form FORTRANTYPE(C_QUALIFIER)
// or just of the form C_QUALIFIER. True means it needs the FORTRANTYPE(C_QUALIFIER)
// format. In the case of a problem (anonymous or unrecognized type), the bool
// problem is set to true. It also catches any type mentioning "va_list".
string CToFTypeFormatter::getFortranTypeASString(bool typeWrapper, bool &problem) {
  string f_type = "";
  problem = false;  // Set this for safety. The caller should also set it to false.

  // Handle character types
  if (c_qualType.getTypePtr()->isCharType()) {
    if (typeWrapper) {
      f_type = "CHARACTER(C_CHAR)";
    } else {
      f_type = "C_CHAR";
    }
  // Handle boolean types
  } else if (c_qualType.getTypePtr()->isBooleanType()) {
    if (typeWrapper) {
      f_type = "LOGICAL(C_BOOL)";
    } else {
      f_type = "C_BOOL";
    }
  // Handle integer types
  } else if (c_qualType.getTypePtr()->isIntegerType()) {
     // Match the necessary integer type with string matching because
     // different types may have the same size in bytes.
     // Handle a size_t
     if (c_qualType.getAsString()== "size_t") {
      if (typeWrapper) {
        f_type = "INTEGER(C_SIZE_T)";
      } else {
        f_type = "C_SIZE_T";
      }
    // Handle integers which are 'chars'
    } else if (c_qualType.getAsString()== "unsigned char" || c_qualType.getAsString()== "signed char") {
      if (typeWrapper) {
        f_type = "INTEGER(C_SIGNED_CHAR)";
      } else {
        f_type = "C_SIGNED_CHAR";
      }        
    // Handle a short type
    } else if (c_qualType.getAsString().find("short") != std::string::npos) {
      if (typeWrapper) {
        f_type = "INTEGER(C_SHORT)";
      } else {
        f_type = "C_SHORT";
      }  
    // Handle a long long
    } else if (c_qualType.getAsString().find("long long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "INTEGER(C_LONG_LONG)";
      } else {
        f_type = "C_LONG_LONG";
      }
    // Handle a regular long
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "INTEGER(C_LONG)";
      } else {
        f_type = "C_LONG";
      }      
    // There are other types of ints, but a type of INT is assumed.
    } else {
      if (typeWrapper) {
        f_type = "INTEGER(C_INT)";
      } else {
        f_type = "C_INT";
      }      
    }

  // Handle the translation of all REAL types.
  } else if (c_qualType.getTypePtr()->isRealType()) {
    // Handle translation of a long double
    if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "REAL(C_LONG_DOUBLE)";
      } else {
        f_type = "C_LONG_DOUBLE";
      } 
    // Handle translation of a float
    } else if (c_qualType.getAsString()== "float") {
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT)";
      } else {
        f_type = "C_FLOAT";
      }
    // Handle translation of a float_128
    } else if (c_qualType.getAsString()== "__float128") {
      if (typeWrapper) {
        f_type = "REAL(C_FLOAT128)";
      } else {
        f_type = "C_FLOAT128";
      }
    // Assume that this "other" kind of real is a double
    } else {
      if (typeWrapper) {
        f_type = "REAL(C_DOUBLE)";
      } else {
        f_type = "C_DOUBLE";
      }      
    }
  // Handle translation of a C derived complex type
  } else if (c_qualType.getTypePtr()->isComplexType ()) {
    // Handle translation of a complex float
    if (c_qualType.getAsString().find("float") != std::string::npos) {
      if (typeWrapper) {
        f_type = "COMPLEX(C_FLOAT_COMPLEX)";
      } else {
        f_type = "C_FLOAT_COMPLEX";
      }        
    // Handle translation of a complex long double
    } else if (c_qualType.getAsString().find("long") != std::string::npos) {
      if (typeWrapper) {
        f_type = "COMPLEX(C_LONG_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_LONG_DOUBLE_COMPLEX";
      }
    // Assume that this is a complex double type and handle it as such
    } else {
      if (typeWrapper) {
        f_type = "COMPLEX(C_DOUBLE_COMPLEX)";
      } else {
        f_type = "C_DOUBLE_COMPLEX";
      }
    }
  // Translate a C pointer
  } else if (c_qualType.getTypePtr()->isPointerType ()) {
    // C function pointers are translated to a special type
    if (c_qualType.getTypePtr()->isFunctionPointerType()){
      if (typeWrapper) {
        f_type = "TYPE(C_FUNPTR)";
      } else {
        f_type = "C_FUNPTR";
      }
    // All other data pointers are translated here
    } else {
      if (typeWrapper) {
        f_type = "TYPE(C_PTR)";
      } else {
        f_type = "C_PTR";
      }
    }
  // Translate a structure declaration. We will recieve
  // the declaration in the form "struct structname" and 
  // will have to deal with the space between the words.
  // Unions have no interoperable Fortran incaranation but they
  // are translated as TYPE's and are handled here as well.
  } else if (c_qualType.getTypePtr()->isStructureType() ||
	     c_qualType.getTypePtr()->isUnionType()) {
    f_type = c_qualType.getAsString();

    // We need to deal with the potential of an anonymous struct
    // or union. This will determine if that is the case by searching for
    // the string "anonymous" which is included in anon types.
    bool anon = false;
    if (f_type.find("anonymous") != std::string::npos) {
      anon = true;
    }

    // This, and it seems all associated types, are C builtins
    // with no Fortran equivalent. In the case that we are to
    // eliminate bad types, we had best deal with these.
    if (f_type.find("__va_list_tag") != std::string::npos) {
      problem = true;
    }

    // We cannot have a space in a fortran type. Erase up
    // to the space. Remember to do this after finding the "anonymous at"
    size_t found = f_type.find_first_of(" ");
    while (found != string::npos) {
      f_type.erase(0, found + 1);  // Erase up to the space
      found=f_type.find_first_of(" ");
    }
    // Fix an illegal name, but a warning would be redundant so do not
    // give one.
    if (f_type.front() == '_') {
      f_type = "h2m" + f_type;  // Prepend h2m to fix the naming problem
    }

    if (typeWrapper) {
      f_type = "TYPE(" + f_type + ")";
    } 
    // Any usage of an anonymous struct is warned about and highighted
    // Note that because of the way the leading spaces are removed, the
    // leading parenthesis is stripped from the anonymous type so a
    // trailing parenthesis is not added in (this will balance the parenthesis.)
    if (anon == true) {
      problem = true;
      f_type = "WARNING_ANONYMOUS(" + f_type;
    }

  // Handle an array type declaration
  } else if (c_qualType.getTypePtr()->isArrayType()) {
    const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
    QualType e_qualType = at->getElementType ();
    // Call this function again on the type found inside the array
    // declaration. This recursion will determine the correct base type.
    // There is another way to do this (getBaseElementType) but this works
    // so let sleeping dogs lie.
    CToFTypeFormatter etf(e_qualType, ac, sloc, args);
    f_type = etf.getFortranTypeASString(typeWrapper, problem);
  // We do not know what the type is. We print a warning and special
  // text around the type in the output file.
  } else {
    f_type = "WARNING_UNRECOGNIZED(" + c_qualType.getAsString()+")";
    problem = true;
  }
  return f_type;
};


// Determines whether or not an input string resembles an integer.
// Useful in determining how to translate a C macro because macros
// do not have types. It looks for pure numbers and for various
// type/format specifiers that might be present in an "int."
// Binary, hex, and octal numbers are checked for specially
// using helper functions.
bool CToFTypeFormatter::isIntLike(const string input) {
  // "123L" "18446744073709551615ULL" "18446744073709551615UL" 
  
  if (std::all_of(input.begin(), input.end(), ::isdigit)) {
    return true;  // All digits: this is an int of some kind
  } else if (isHex(input) == true) {  // This is a hexadecimal.
    return true;
  } else if (isBinary(input) == true) {  // This is a binary number.
    return true;
  } else if (isOctal(input) == true) {  // This is an octal number.
    return true;
  } else {
    string temp = input;
    // These are double like digits, unacceptable in an int-like
    // Note that F/f is alright in a hex number, but those have
    // already been dealt with.
    size_t doubleF = temp.find_first_of(".eFf");
    if (doubleF != std::string::npos) {
      return false;
    }    
    
    // If there are no digits, it is not a number. Hex numbers might
    // have no digits, but those are dealt with above.
    size_t found = temp.find_first_of("01234567890");
    if (found == std::string::npos) {
      return false;
    }

    // This erases non-digit characters hoping to weed
    // down to the number at the core.
    while (found != std::string::npos)
    {
      temp.erase(found, found+1);
      found=temp.find_first_of("01234567890");
    }
    // We have erased all the digits and now see the suffixes. 
    // Hexadecimals have already been dealt with, so an x is
    // not acceptable in the string.
    if (!temp.empty()) {
      found = temp.find_first_of("UuLl()- ");
      while (found != std::string::npos) {
        temp.erase(found, found+1);
        found=temp.find_first_of("UuLl()- ");
      }
    }
    // If it is empty after erasing suffixes, it is int like
    // because it just contained digits and suffixes.
    return temp.empty();
  }
};

// Determines whether or not the string input resembles a
// double. Useful for determining how to translate a C macro
// which doesn't have a type. It looks for a decimal place
// and digits and also handles various letters that might
// indicate type/format of a double-like entity.
bool CToFTypeFormatter::isDoubleLike(const string input) {
  // "1.23", 1.18973149535723176502e+4932L
  string temp = input;
  // If we can't find a digit, we know it's not double like
  size_t found = temp.find_first_of("01234567890");
  if (found==std::string::npos) {
    return false;
  }
  // Erase all the digits from the string.
  while (found != std::string::npos) {
    temp.erase(found, found+1);
    found=temp.find_first_of("01234567890");
  }
  // Now that all the digits are erased, we look at the suffixes,
  // e/E for exponents, . for decimals, and ()/+/- which may
  // appear. Note that this isn't full-proof. It's more of a
  // best guess. Malicious numbers could fool it.
  if (!temp.empty()) {
    size_t doubleF = temp.find_first_of(".eEfFuUlL()+- ");
    while (doubleF != std::string::npos) {
      temp.erase(doubleF, doubleF+1);
      doubleF=temp.find_first_of(".eEfFuUlL()+- ");
    }
  }
  // If nothing remains after erasing the suffixes, we
  // have found a double like entity.
  return temp.empty();
};

// Returns true if the string under consideration is
// a hexadecimal constant. A hex constant has the form
// 0xa12edf or 0XA12Edf with unsigned or long specifiers,
// potentially. Note that a - sign is not allowed.
bool CToFTypeFormatter::isHex(const string in_str) {
  string input = in_str;
  // Remove all leading spaces from the macro.
  while (input[0] == ' ' || input[0] == '(') {
    input.erase(input.begin(), input.begin() + 1);
  }

  if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
    // Erase the 0x or 0X from the begining.
    input.erase(input.begin(), input.begin() + 2);
    size_t found = input.find_first_of("01234567890abcdefABCDEF");
    // Loop through, erasing hex digits as they are found.
    while (found != std::string::npos) {
      if (found != 0) {  // The first digit should always be hex.
        break;  // Break out of the loop to inspect the remainders.
      } 
      // Continue the iteration. It may be a hexadecimal.
      input.erase(found, found + 1);
      found = input.find_first_of("01234567890abcdefABCDEF");
    }
  } else {  // The first digits did not match 0x or 0X
    return false;
  }
  // Strip of unsigned or long specifiers
  if (input.empty() == false) {
    size_t found = input.find_first_of("UuLl() ");
      while (found != std::string::npos)
      {
        input.erase(found, found+1);
        found=input.find_first_of("UuLl() ");
      }
   }
  // If the string was all hex constants and modifiers, we are good.
  return input.empty();
}

// Returns true if the string under consideration is
// a binary constant. These are of the form 0b01101
// or 0B011011 and may have long or unsigned specifiers.
// Note a - sign is not allowed.
bool CToFTypeFormatter::isBinary(const string in_str) {
  string input = in_str;
  // Remove all leading spaces from the macro.
  while (input[0] == ' ' || input[0] == '(') {
    input.erase(input.begin(), input.begin() + 1);
  }
  if (input[0] == '0' && (input[1] == 'b' || input[1] == 'B')) {
    // Erase the 0b or 0B from the begining.
    input.erase(input.begin(), input.begin() + 2);
    // Erase binary digits until none can be found.
    while (input[0] == '0' || input[0] == '1') {
      input.erase(input.begin(), input.begin() + 1);
    }
  } else {  // The first two digits are not 0b or 0B
    return false;
  }
  // Strip of unsigned or long specifiers
  if (input.empty() == false) {
    size_t found = input.find_first_of("UuLl() ");
      while (found != std::string::npos) {
        input.erase(found, found+1);
        found=input.find_first_of("UuLl() ");
      }
   }

  // If the string was all binary, we are good.
  return input.empty();
}

// Returns true if the string under consideration is an
// octal constant. These are of the form 0123843
// with potentially unsigned or long specifiers.
// Note a - sign is not allowed.
bool CToFTypeFormatter::isOctal(const string in_str) {
  string input = in_str;
  // Remove all leading spaces from the macro.
  while (input[0] == ' ' || input[0] == '(') {
    input.erase(input.begin(), input.begin() + 1);
  }
  // If the string is empty or just '0', it is not octal.
  if (input.length() <= 1) {
    return false;
  }
  if (input[0] == '0') {
    // Erase the 0 from the begining.
    input.erase(input.begin(), input.begin() + 1);
    // This is the case if there were a 0U or 0L, which
    // is not an octal digit, but might look like one.
    if (isdigit(input[0]) == false) {
      return false;
    }

    // Erase all the octal digits.
    size_t found = input.find_first_of("01234567");
    while (found != std::string::npos) {
      if (found != 0) {  // The first digit should always be octal..
        return false;
      } 
      // Continue the iteration. It may be a octal..
      input.erase(found, found + 1);
      found = input.find_first_of("01234567");
    }
  } else {  // It does not begin with '0'.
    return false;
  }
  // Strip of unsigned or long specifiers
  if (input.empty() == false) {
    size_t found = input.find_first_of("UuLl() ");
      while (found != std::string::npos)
      {
        input.erase(found, found+1);
        found=input.find_first_of("UuLl() ");
      }
   }

  // If the string was all octal, we are good.
  return input.empty();
}

// Determines whether or not an input string can be
// treated as a string constant ("abc"). Useful 
// for determining how to translate a C macro
// which has no type.
bool CToFTypeFormatter::isString(const string input) {
  string s = input;
  // Strip off leading spaces.
  while (s[0] == ' ') {
    s = s.substr(1);
  }
  // Check to make sure that there are enclosing quotes.
  if (s[0] == '\"' && s[s.size()-1] =='\"') {
    return true;
  }
  return false;
};

// Determines whether or not an input string can be
// treated as a a C character ('a'). Useful for determining
// how to translate a C macro which has no type.
bool CToFTypeFormatter::isChar(const string input) {
  string s = input;
  // Strip off leading spaces.
  while (s[0] == ' ') {
    s = s.substr(1);
  }
  // Check to make sure there are enclosing quotes.
  if (s[0] == '\'' && s[s.size()-1] =='\'') {
    return true;
  }
  return false;
};

// Returns true if the string input provided contains the
// word short, int, long, or char. Otherwise it returns false.
// Useful for determining how to translate a C macro which 
// does not have a type.
bool CToFTypeFormatter::isType(const string input) {
  // only support int short long char for now
  if (input == "short" || input == "long" || input == "char" || input == "int" ||
      input.find(" int") != std::string::npos || 
      input.find(" short") != std::string::npos || 
      input.find(" long") != std::string::npos || 
      input.find(" char") != std::string::npos) {
    return true;
  }
  return false;
};

// Returns a string buffer containing the Fortran equivalent of a C macro resembling a type
// definition. The Arguments and PresumedLoc are used to give information about the location of any
// errors that might occur during an attempted translation. The macro is translated into 
// a Fortran TYPE definition. This only supports int, shorts, chars, and longs. The TYPE
// will include one field which is [type_name]_C_CHAR/C_INT/C_LONG as appropriate given
// the type being declared. 
string CToFTypeFormatter::createFortranType(const string macroName, const string macroVal,
    PresumedLoc loc, Arguments &args) {
  string ft_buffer;
  string type_id = "typeID_" + macroName ;
  string temp_macro_name = macroName;  // The macroName string is const
  
  // Create a new name for the transalated type.
  // The name may not include spaces. Erase them if they are there.
  size_t found = type_id.find_first_of(" ");
  while (found != string::npos) {
    type_id.erase(0, found + 1);  // Erase up to the space
    found=type_id.find_first_of(" ");
  }

  // We have found an illegal name. We deal with it as usual by prepending h2m.
  if (macroName[0] == '_') {
    CToFTypeFormatter::PrependError(macroName, args, loc);
    temp_macro_name = "h2m" + macroName;
  }

  ft_buffer = "TYPE, BIND(C) :: " + temp_macro_name + "\n";
  if (macroVal.find("char") != std::string::npos) {
    ft_buffer += "    CHARACTER(C_CHAR) :: " + type_id + "\n";
  } else if (macroVal.find("long") != std::string::npos) {
    ft_buffer += "    INTEGER(C_LONG) :: " + type_id + "\n";
  } else if (macroVal.find("short") != std::string::npos) {
    ft_buffer += "    INTEGER(C_SHORT) :: " + type_id + "\n";
  } else {
    ft_buffer += "    INTEGER(C_INT) :: " + type_id + "\n";
  }
  ft_buffer += "END TYPE " + temp_macro_name + "\n";

  return ft_buffer;
};

// Given a string describing an integer like macro, determine what size modifier to
// give to the corresponding Fortran type. The boolean value will be used to pass
// back a failure if there are too many appearances of L or U
string CToFTypeFormatter::DetermineIntegerType(const string integer_in, bool &invalid) {
  invalid = false;
  string instr = integer_in;
  int total_l = 0;
  int total_u = 0;
  size_t found = instr.find_first_of("Ll");
  // Count the number of appearnces of L or l in the string
  while (found != std::string::npos) {
    ++total_l;
    found = instr.find_first_of("Ll", found+1);
  }
  // We ignore the signed specifiers, but make sure there
  // are not too many of them.
  found = instr.find_first_of("Uu");
  while (found != std::string::npos) {
    ++total_u;
    found = instr.find_first_of("Uu", found+1);
  } 
  if (total_l > 2 || total_u > 1) {
    invalid = true;
    return "C_INVALID";
  }
  // Based on the number of "l" or "L" modifiers, determine the size
  if (total_l == 1) {  // A single long modifier was found
    return "C_LONG";
  } else if (total_l == 2) {  // Two long modifiers were found
    return "C_LONG_LONG";
  }
  return "C_INT";  // Integer size is the default.
}

// Similar to the function above, this will determine what size modifier to give
// to the corresponding fortran type when presented with a double-like string value.
// The boolean argument is used to pass failure information back if there are too
// many appearances of L/l or U/u
string CToFTypeFormatter::DetermineFloatingType(const string floating_in, bool &invalid) {
  invalid = false;
  string instr = floating_in;
  int total_l = 0;  // Count long specifiers
  int total_u = 0;  // Count unsigned specifiers
  int total_f = 0;  // Count float type specifiers
  int total_e = 0;  // This is th exponent specifier; we can't have more than one.
  size_t found = instr.find_first_of("Ll");
  // Count the number of appearnces of L or l in the string
  while (found != std::string::npos) {
    ++total_l;
    found = instr.find_first_of("Ll", found+1);
  }
  // We ignore the signed specifiers, but make sure there
  // are not too many of them.
  found = instr.find_first_of("Uu");
  while (found != std::string::npos) {
    ++total_u;
    found = instr.find_first_of("Uu", found+1);
  }
  // Now we count the number of appearances of e/E.
  // There cannot be more than one.
  found = instr.find_first_of("Ee");
  while (found != std::string::npos) {
    ++total_e;
    found = instr.find_first_of("Ee", found+1);
  }
  // We count the appearances of f. There should only be one.
  found = instr.find_first_of("Ff");
  while (found != std::string::npos) {
    ++total_f;
    found = instr.find_first_of("Ff", found+1);
  } 
  // More than one float, long, or unsigned modifier is not supported.
  // More than one e/E exponenet specifier is illegal.
  if (total_f > 1 || total_u > 1 || total_l > 1 || total_e > 1) {
    invalid = true;
    return "C_INVALID";
  }

  // A long (L/l) combined with an float (f/F) is illegal
  if (total_f > 0 && total_l > 0) {
    invalid = true;
    return "C_INVALID";
  } else if (total_f > 0) {  // A float specifier is found.
    return "C_FLOAT";
  } else if (total_l > 0) {  // A long specifier is found.
    return "C_LONG_DOUBLE";
  }
  return "C_DOUBLE";  // Double is the default. 
}

// This function removes questionable characters from a
// floating point string by culling anything that isn't a digit
// or the '.' character, e/E or '-'.
string CToFTypeFormatter::GroomFloatingType(const string in) {
  string val = "";
  // Search through the input string and retrieve all the numbers. 
  // Also keep the '.' character.
  int i = 0;
  for (i = 0; i < in.length(); i++) {
    // Check that each member is a digit. Add it on if it is.
    // Also keep the '.', 'e'/'E' for exponents, and '-'.
    if (isdigit(in[i]) || in[i] == '.' || in[i] == '-' ||
        in[i] == 'e' || in[i] == 'E') {
      val = val + in[i];
    }
  }
  return val; 
}

// This function removes questionable characters from an integer
// string by removing everything that is not a digit or '-'.
string CToFTypeFormatter::GroomIntegerType(const string in) {
  int i = 0;
  string val = "";
  // Search through the input string and retrieve all the numbers.
  for (i = 0; i < in.length(); i++) {
    // Check that each member is a digit. Add it on if it is.
    // Also keep the '-' character.
    if (isdigit(in[i]) || in[i] == '-') {
      val = val + in[i];
    }
  }
  return val; 
}

// This function removes questionable characters from a
// hexadecimal (removes EVERYTHING that isn't a hex character).
string CToFTypeFormatter::GroomHexType(const string in) {
  int i = 0;
  string val = "";
  // Search through the input string and add on only hex
  // digits to the growing string.
  for (i = 0; i < in.length(); i++) {
    if (isxdigit(in[i])) {
      val = val + in[i];
    }
  }
  return val; 
}
