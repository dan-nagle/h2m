// This file contains the MacroFormatter class for the h2m
// translator.

#include "h2m.h"
//-----------formatter functions----------------------------------------------------------------------------------------------------

// -----------initializer MacroFormatter--------------------
// The preprocessor is used to find the macro names in the source files. The macro is 
// more difficult to process. Both its name and definition are fetched using the lexer.
MacroFormatter::MacroFormatter(const Token MacroNameTok, const MacroDirective *md, 
    CompilerInstance &ci, Arguments &arg) : md(md), args(arg), ci(ci) {
  current_status = CToFTypeFormatter::OKAY;
  error_string = "";
  const MacroInfo *mi = md->getMacroInfo();
  SourceManager& SM = ci.getSourceManager();

  // define macro properties
  isObjectOrFunction = mi->isObjectLike();
 
  // This should be a fine way to deal with invalid locations because sloc is checked for validity
  // before it is used.
  if (mi->getDefinitionLoc().isValid()) {
    sloc = SM.getPresumedLoc(mi->getDefinitionLoc());
    isInSystemHeader = SM.isInSystemHeader(mi->getDefinitionLoc());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }

  // The macros' source text is fetched using the Lexer
  macroName = Lexer::getSourceText(CharSourceRange::getTokenRange(MacroNameTok.getLocation(),
      MacroNameTok.getEndLoc()), SM, LangOptions(), 0);
  macroDef = Lexer::getSourceText(CharSourceRange::getTokenRange(mi->getDefinitionLoc(),
      mi->getDefinitionEndLoc()), SM, LangOptions(), 0);
  
  // strangely there might be a "(" follows the macroName for function macros,
  // remove it if there is
  if (macroName.back() == '(') {
    macroName.erase(macroName.size()-1);
  }

  // get the value (number or char or string) for an object macro
  // Note that this doesn't really work properly because of other forms
  // of whitespace and all helpers also look for whitespace in macros.
  bool frontSpace = true;
  for (size_t i = macroName.size(); i < macroDef.size(); i++) {
    if (macroDef[i] != ' ') {
      frontSpace = false;
      macroVal += macroDef[i];
    } else if (frontSpace == false) {
      macroVal += macroDef[i];
    }
  }
}

bool MacroFormatter::isObjectLike() {
  return isObjectOrFunction;
};
bool MacroFormatter::isFunctionLike() {
  return !isObjectOrFunction;
};

// return the entire macro in fortran
// Macros which are object like, meaning similar to Chars, Strings, integers, doubles, etc
// can be translated. Macros which are empty are defined as positive bools. Function-like
// macros can be translated as functions or subroutines. However, it is not always possible
// to translate a macro, in which case the line must be commented out. There is also an 
// option that may be invoked to request that all function-like macros be commented out.
// This option is caried out later. The status flag is set to show that the macro is
// function like. The central error handling function will deal with commenting it out
// if need be.
string MacroFormatter::getFortranMacroASString() {
  string fortranMacro;

  // If we are not in the main file, don't include this. Just
  // return an empty string.  If the Together argument is specified, include it anyway.
  if (ci.getSourceManager().isInMainFile(md->getMacroInfo()->getDefinitionLoc()) == false
      && args.getTogether() == false) {
    return "";
  } 
  if (!isInSystemHeader) {  // Keeps macros from system headers from bleeding into the file
    // remove all tabs from the macro definition.
    macroVal.erase(std::remove(macroVal.begin(), macroVal.end(), '\t'), macroVal.end());
    // Warn about the presence of an illegal underscore at the beginning of a name.
    string actual_macroName = macroName;  // We may need to prepend h2m to the beginning.
    if (macroName[0] == '_') {
      CToFTypeFormatter::PrependError(macroName, args, sloc);
      actual_macroName = "h2m" + macroName;
    }

    // handle object first, this means definitions of parameters of int, char, double... types
    if (isObjectLike()) {
      // analyze the macro's type and translate as appropriate
      if (!macroVal.empty()) {
        if (CToFTypeFormatter::isString(macroVal)) {
          fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+
              "), parameter, public :: " + actual_macroName + " = " + macroVal + "\n";
        } else if (CToFTypeFormatter::isChar(macroVal)) {
          fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+
              "), parameter, public :: "+ macroName + " = " + macroVal + "\n";
        } else if (CToFTypeFormatter::isIntLike(macroVal)) {
          // Will hold the type_needed of INTEGER(type_needed), parameter...
          string type_specifier = "";
          bool invalid = false;
          type_specifier = CToFTypeFormatter::DetermineIntegerType(macroVal, invalid);
          // invalid will be true if the proper type couldn't be detmined
          if (invalid == true) {  // The specifiers cannot be handled.
            current_status = CToFTypeFormatter::BAD_MACRO;
            error_string = actual_macroName + ", " + macroVal;
            return macroDef;  // We can skip the name and line checks down below.
          }
          // Build up the part of the macro we already know
          fortranMacro = "INTEGER(" + type_specifier + "), parameter, public :: ";
          // Handle hexadecimal constants (0x or 0X is discovered in the number)
          if (CToFTypeFormatter::isHex(macroVal) == true) {
            size_t x = macroVal.find_last_of("xX");
            string val = macroVal.substr(x+1);
            // Strip val down to its pure hexadecimal digits.
            val = CToFTypeFormatter::GroomHexType(val);
            fortranMacro += actual_macroName + " = Z\'" + val + "\'\n";
          // Handle a binary constant (0B or 0b is discovered in the number)
          } else if (CToFTypeFormatter::isBinary(macroVal) == true) {
            size_t b = macroVal.find_last_of("bB");
            string val = macroVal.substr(b+1);
            // Remove questionable characters from the number.
            val = CToFTypeFormatter::GroomIntegerType(val);
            fortranMacro += actual_macroName + " = B\'" + val + "\'\n";
          // We have found an octal number: 0####
          } else if (CToFTypeFormatter::isOctal(macroVal) == true) {
            string val = macroVal;
            // Remove the leading zero.
            val.erase(val.begin(), val.begin() + 1);
            // Remove questionable characters from the number.
            val = CToFTypeFormatter::GroomIntegerType(val);
            fortranMacro += actual_macroName + " = O\'" + val + "\'\n";
          } else {  // This is some other kind of integer like number.
            string val = macroVal;  // Create a mutable temporary string.
            // Remove questionable characters from the number.
            val = CToFTypeFormatter::GroomIntegerType(val);
            fortranMacro += actual_macroName + " = " + val + "\n";
          }
        } else if (CToFTypeFormatter::isDoubleLike(macroVal)) {
          string type_specifier = "";
          bool invalid = false;
          type_specifier = CToFTypeFormatter::DetermineFloatingType(macroVal, invalid);
          // Some illegal modifiers have been discovered in the macro and
          // we can't determine its type.
          if (invalid == true) {
            current_status = CToFTypeFormatter::BAD_MACRO;
            error_string = actual_macroName + ", " + macroVal;
            return macroDef;
          }
          // Remove questionable characters from the decimal.
          string val = CToFTypeFormatter::GroomFloatingType(macroVal);
          fortranMacro = "REAL(" + type_specifier + "), parameter, public :: " +
              actual_macroName + " = " + val + "\n";
        // Be aware that this may create multiline macros. All others
        // created here will be single line macros, so this can be a
        // surprise. This comes into play when someone types:
        // "#define some_weird_thing int".
        } else if (CToFTypeFormatter::isType(macroVal)) {
          // only support int short long char for now
          fortranMacro = CToFTypeFormatter::createFortranType(actual_macroName,
               macroVal, sloc, args);
        // We do not know what to do with this object like macro, so we comment it out.
        } else {
          current_status = CToFTypeFormatter::BAD_MACRO;
          error_string = actual_macroName + ", " + macroVal;
          return macroDef;  // We can skip the name and line checks down below.
        }
      } else { // The macro is empty, so, make the object a bool positive
        fortranMacro = "INTEGER(C_INT), parameter, public :: "+ actual_macroName  + " = 1\n";
      }
    } else {  // We are dealing with a function macro.
      // macroDef has the entire macro definition in it. Here the body of the macro
      // is parsed out.
      current_status = CToFTypeFormatter::FUNC_MACRO;
      size_t rParen = macroDef.find(')');
      string functionBody = macroDef.substr(rParen+1, macroDef.size()-1);
      fortranMacro = "INTERFACE\n";
      if (md->getMacroInfo()->arg_empty()) {
        fortranMacro += "SUBROUTINE "+ actual_macroName + "() BIND(C)\n";
      } else {
        fortranMacro += "SUBROUTINE "+ actual_macroName + "(";
        for (auto it = md->getMacroInfo()->arg_begin (); it !=
            md->getMacroInfo()->arg_end (); it++) {
          // Assemble the macro arguments in a list and check names for illegal underscores. 
          string argname = (*it)->getName();
          if (argname.front() == '_') {
            CToFTypeFormatter::PrependError(macroName, args, sloc);
            argname = "h2m" + argname;  // Fix the illegal name problem by prepending h2m
          }
          fortranMacro += argname;  // Add the new argument into the subroutine's definition.
          fortranMacro += ", ";
        }
        // erase the redundant comma and space at the end of the macro
        fortranMacro.erase(fortranMacro.size()-2);
        fortranMacro += ") BIND(C)\n";
      }
      // Comment out the body of the function using the standard string-stream idiom.
      if (!functionBody.empty()) {
        std::istringstream in(functionBody);
        // Iterate through the function body line by line.
        for (std::string line; std::getline(in, line);) {
          if (args.getSilent() == false && args.getQuiet() == false) {
            errs() << "Warning: line " << line << " commented out.\n";
            CToFTypeFormatter::LineError(sloc);
          }
          fortranMacro += "! " + line + "\n";
        }
      }
      fortranMacro += "END SUBROUTINE " + actual_macroName + "\n";
      fortranMacro += "END INTERFACE\n";
    }

    // Checks for illegal name lengths are done in one place. This is 
    // so much nicer than how it used to be.
    // Check the name's length to make sure that it is valid
    if (actual_macroName.length() >= CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = actual_macroName + ", macro name.";
    }
    // Now check to see if this is a repeated identifier. This is very uncommon but could occur.
    if (RecordDeclFormatter::StructAndTypedefGuard(actual_macroName) == false) {
      current_status = CToFTypeFormatter::DUPLICATE;
      error_string = actual_macroName + ", macro name.";
    }
    // Check line lengths on all macro lines. It is best do do this in one place.
    std::istringstream in(fortranMacro);
    // Iterate through the macro line by line with the
    // standard string stream logic.
    for (std::string line; std::getline(in, line);) {
      if (line.length() > CToFTypeFormatter::line_max) {
        current_status = CToFTypeFormatter::BAD_LINE_LENGTH;
        error_string = actual_macroName + ", " + line;
      }
    }
  }
  return fortranMacro;
};

