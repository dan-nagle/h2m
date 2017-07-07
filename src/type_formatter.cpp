// This file holds the code for the CToFTypeFormatter class
// for the h2m autofortran tool. 

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

// -----------initializer RecordDeclFormatter--------------------
CToFTypeFormatter::CToFTypeFormatter(QualType qt, ASTContext &ac, PresumedLoc loc, Arguments &arg): ac(ac), args(arg) {
  c_qualType = qt;
  sloc = loc;
};

// Determines whether the qualified type offered is identical to that it is called on.
// Pointer types are only distinguished in terms of function vs data pointers. 
bool CToFTypeFormatter::isSameType(QualType qt2) {
  // for pointer type, only distinguish between the function pointer from other pointers
  if (c_qualType.getTypePtr()->isPointerType() && qt2.getTypePtr()->isPointerType()) {
    // True if both are function pointers
    if (c_qualType.getTypePtr()->isFunctionPointerType() && qt2.getTypePtr()->isFunctionPointerType()) {
      return true;
    // True if both are not function pointers
    } else if ((!c_qualType.getTypePtr()->isFunctionPointerType()) && (!qt2.getTypePtr()->isFunctionPointerType())) {
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
// determine an array suffix by determining the type of the array, the element
// size, and the length in order to provide a declaration.
string CToFTypeFormatter::getFortranIdASString(string raw_id) {
  // Determine if it needs to be substituted out
  if (c_qualType.getTypePtr()->isArrayType()) {
    // In this case, the array bounds are fixed and we can determine the
    // exact dimensions of the declared array.
    if (c_qualType.getTypePtr()->isConstantArrayType()) {
      // The helper fetches the dimensions in the form "x, y, z"
      raw_id += "(" + getFortranArrayDimsASString() + ")";
    } else {  // This is not necessarilly a constant length array.
      // Only single dimension arrays are handled correctly here.
      const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
      QualType e_qualType = at->getElementType ();
      int typeSize = ac.getTypeSizeInChars(c_qualType).getQuantity();
      int elementSize = ac.getTypeSizeInChars(e_qualType).getQuantity();
      int numOfEle = typeSize / elementSize;
      string arr_suffix = "(" + to_string(numOfEle) + ")";
      raw_id += arr_suffix;
    }
  }
  return raw_id;
};

// This function will return the raw dimensions of an array as a comma separated
// list. If requested on the command line, the dimensions will be reversed.
// In the case that the array does not have constant dimensions, proper syntax
// for an assumed shape array will be employed.
string  CToFTypeFormatter::getFortranArrayDimsASString() {
  string dim_str = "";  // String of dimensions to return.

  QualType element_type = c_qualType;  // Element type of the array for looping.
  std::stack<string> dimensions;  // Holds dimensions so they can be reversed
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
      // This if statement either prepares a stack to reverse the
      // dimensions of the array (if requested) or else adds them
      // into place without reversal
      if (args.getArrayTranspose() == true) {
        dimensions.push(cat->getSize().toString(10, true));
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
          dimensions.push(eval_str);
        } else {
          dim_str += eval_str + ", ";
        }
      // If an array is declared as * size, we can just add "*, ". There are three
      // possible array size modifiers, Normal, Star, and Static (func decls only)
      } else if (vat->getSizeModifier() == ArrayType::ArraySizeModifier::Star) {
        if (args.getArrayTranspose() == true) {
          dimensions.push("*, ");
        } else {
          dim_str += "*, ";
        }
      } else {  // We cannot evaluate the expression. We fetch the source text.
        string expr_text = Lexer::getSourceText(CharSourceRange::getTokenRange(
            vat->getBracketsRange()), ac.getSourceManager(), LangOptions(), 0);
        // This will give us the square brackets, so remove those from the ends
        // because there should be no square brackets in Fortran array dimensions.
        // This length test is to prevent an exception if something weird
        // has happened during the evaluation.
        if (expr_text.length() > 2) {
          expr_text.erase(expr_text.begin(), expr_text.begin() + 1);
          expr_text.erase(expr_text.end()-1, expr_text.end());
        }

        // Put our possibly illegal expression into place with the other dimensions.
        if (args.getArrayTranspose() == true) {
          dimensions.push(expr_text);
        } else {
          dim_str += expr_text + ", ";
        }
        // This is likely a serious issue. It may prevent compilation.
        if (args.getSilent() == false) {
          errs() << "Warning: unevaluatable array dimensions: " << expr_text << "\n";
          LineError(sloc);
        }
      }
    
      // Get information about the next dimension of the array and repeat.
      element_type = vat->getElementType();
      the_type_ptr = element_type.getTypePtr();
    // An incomplete type array has an unspecified size ie "arr[]"
    } else if (the_type_ptr->isIncompleteArrayType()) {
      return "*";  // Return the syntax for a variable size array.
    }
  }
  // There is one other possibility: DependentSizedArrayType, but this is a C++
  // construct that h2m does not support (DSAT's are template based arrays).

  // Build up the raw ID from the stack, reversing array dimensions
  // if requested and the stack was built
  if (args.getArrayTranspose() == true) {
    while (dimensions.empty() == false) {
      dim_str += dimensions.top() + ", ";
      dimensions.pop(); 
    }
  }
  // We erase the extra space and comma from the last loop iteration.
  dim_str.erase(dim_str.end() - 2, dim_str.end());
  return dim_str;
}

// This function will create a specification statement for a Fortran array argument
// as might appear in a function or subroutine prototype, (ie for "int thing (int x[5])"
// it returns "INTEGER(C_INT), DIMENSION(5) :: x").
string CToFTypeFormatter::getFortranArrayArgASString(string dummy_name) {
  // Start out with the type (ie INTEGER(C_INT))
  string arg_buff = getFortranTypeASString(true) + ", DIMENSION(";
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
// format.
string CToFTypeFormatter::getFortranTypeASString(bool typeWrapper) {
  string f_type;

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
    // Handle a long or a long long. Long longs are ignored here. The type is assumed to be long.
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
  // Unions have no interoperable Fortran incranation but they
  // are translated as TYPE's and are handled here as well.
  } else if (c_qualType.getTypePtr()->isStructureType() ||
	     c_qualType.getTypePtr()->isUnionType()) {
    f_type = c_qualType.getAsString();
    // We cannot have a space in a fortran type. Erase up
    // to the space.
    size_t found = f_type.find_first_of(" ");
    while (found != string::npos) {
      f_type.erase(0, found + 1);  // Erase up to the space
      found=f_type.find_first_of(" ");
    }
    if (f_type.front() == '_') {
      if (args.getSilent() == false) {
        errs() << "Warning: fortran names may not begin with an underscore.";
        errs() << f_type << " renamed " << "h2m" << f_type << "\n";
        LineError(sloc);
      }
      f_type = "h2m" + f_type;  // Prepend h2m to fix the naming problem
    }
    if (typeWrapper) {
      f_type = "TYPE(" + f_type + ")";
    } 
  // Handle an array type declaration
  } else if (c_qualType.getTypePtr()->isArrayType()) {
    const ArrayType *at = c_qualType.getTypePtr()->getAsArrayTypeUnsafe ();
    QualType e_qualType = at->getElementType ();
    // Call this function again on the type found inside the array
    // declaration. This recursion will determine the correct type.
    CToFTypeFormatter etf(e_qualType, ac, sloc, args);
    f_type = etf.getFortranTypeASString(typeWrapper);
  // We do not know what the type is. We print a warning and special
  // text around the type in the output file.
  } else {
    f_type = "unrecognized_type(" + c_qualType.getAsString()+")";
    // Warning only in the case of a typewrapper avoids repetitive error messages
    if (typeWrapper) {
      if (args.getSilent() == false) {
        errs() << "Warning: unrecognized type (" << c_qualType.getAsString() << ")\n";
        LineError(sloc);
      }
    }
  }
  return f_type;
};


// Determines whether or not an input string resembles an integer.
// Useful in determining how to translate a C macro because macros
// do not have types. It looks for pure numbers and for various
// type/format specifiers that might be present in an "int."
bool CToFTypeFormatter::isIntLike(const string input) {
  // "123L" "18446744073709551615ULL" "18446744073709551615UL" 
  if (std::all_of(input.begin(), input.end(), ::isdigit)) {
    return true;
  } else {
    string temp = input;
    size_t doubleF = temp.find_first_of(".eF");
    if (doubleF != std::string::npos) {
      return false;
    }    
    
    // If there are no digits, it is not a number.
    size_t found = temp.find_first_of("01234567890");
    if (found==std::string::npos) {
      return false;
    }

    while (found != std::string::npos)
    {
      temp.erase(found, found+1);
      found=temp.find_first_of("01234567890");
    }
    
    if (!temp.empty()) {
      found = temp.find_first_of("xUL()- ");
      while (found != std::string::npos)
      {
        temp.erase(found, found+1);
        found=temp.find_first_of("xUL()- ");
      }
      return temp.empty();
    } else {
      return false;
    }
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
  size_t found = temp.find_first_of("01234567890");
  if (found==std::string::npos) {
    return false;
  }
  while (found != std::string::npos)
  {
    temp.erase(found, found+1);
    found=temp.find_first_of("01234567890");
  }
  // no digit anymore
  if (!temp.empty()) {
    size_t doubleF = temp.find_first_of(".eFUL()+- ");
    while (doubleF != std::string::npos)
    {
      temp.erase(doubleF, doubleF+1);
      doubleF=temp.find_first_of(".eFUL()+- ");
    }
    return temp.empty();
  } else {
    return false;
  }
  
  if (!temp.empty()) {
    found = temp.find_first_of(".eFL()- ");
    while (found != std::string::npos)
    {
      temp.erase(found, found+1);
      found=temp.find_first_of("xUL()- ");
    }
    return temp.empty();
  } else {
    return false;
  }
};

// Determines whether or not an input string can be
// treated as a string constant ("abc"). Useful 
// for determining how to translate a C macro
// which has no type.
bool CToFTypeFormatter::isString(const string input) {
  string s = input;
  while (s[0] == ' ') {
    s = s.substr(1);
  }
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
  while (s[0] == ' ') {
    s = s.substr(1);
  }
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
    if (args.getSilent() == false) {
      errs() << "Warning: Fortran names may not start with '_' ";
      errs() << macroName << " renamed h2m" << macroName << "\n";
      LineError(loc);
    }
    temp_macro_name = "h2m" + macroName;
  }

  // The CheckLength function is employed here to make sure lines are acceptable lengths
  ft_buffer = CheckLength("TYPE, BIND(C) :: " + temp_macro_name + "\n", CToFTypeFormatter::line_max,
      args.getSilent(), loc);
  if (macroVal.find("char") != std::string::npos) {
    ft_buffer += CheckLength("    CHARACTER(C_CHAR) :: " + type_id + "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
  } else if (macroVal.find("long") != std::string::npos) {
    ft_buffer += CheckLength("    INTEGER(C_LONG) :: " + type_id + "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
  } else if (macroVal.find("short") != std::string::npos) {
    ft_buffer += CheckLength("    INTEGER(C_SHORT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
  } else {
    ft_buffer += CheckLength("    INTEGER(C_INT) :: " + type_id + "\n", CToFTypeFormatter::line_max,
        args.getSilent(), loc);
  }
  ft_buffer += "END TYPE " + temp_macro_name + "\n";

  return ft_buffer;
};

