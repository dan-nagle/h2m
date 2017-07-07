// This file contains the MacroFormatter class for the h2m
// translator.

#include "h2m.h"
//-----------formatter functions----------------------------------------------------------------------------------------------------

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

// -----------initializer MacroFormatter--------------------
// The preprocessor is used to find the macro names in the source files. The macro is 
// more difficult to process. Both its name and definition are fetched using the lexer.
MacroFormatter::MacroFormatter(const Token MacroNameTok, const MacroDirective *md, CompilerInstance &ci,
    Arguments &arg) : md(md), args(arg), ci(ci) {
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

    // source text
    macroName = Lexer::getSourceText(CharSourceRange::getTokenRange(MacroNameTok.getLocation(), MacroNameTok.getEndLoc()), SM, LangOptions(), 0);
    macroDef = Lexer::getSourceText(CharSourceRange::getTokenRange(mi->getDefinitionLoc(), mi->getDefinitionEndLoc()), SM, LangOptions(), 0);
    
    // strangely there might be a "(" follows the macroName for function macros, remove it if there is
    if (macroName.back() == '(') {
      //args.getOutput().os() << "unwanted parenthesis found, remove it \n";
      macroName.erase(macroName.size()-1);
    }

    // get value for object macro
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
// to translate a macro, in which case the line must be commented out.
string MacroFormatter::getFortranMacroASString() {
  string fortranMacro;
  bool duplicateIdentifier = false;

  // If we are not in the main file, don't include this. Just
  // return an empty string.  If the Together argument is specified, include it anyway.
  if (ci.getSourceManager().isInMainFile(md->getMacroInfo()->getDefinitionLoc()) == false
      && args.getTogether() == false) {
    return "";
  } 
  if (!isInSystemHeader) {  // Keeps macros from system headers from bleeding into the file
    // remove all tabs
    macroVal.erase(std::remove(macroVal.begin(), macroVal.end(), '\t'), macroVal.end());

    // handle object first, this means definitions of parameters of int, char, double... types
    if (isObjectLike()) {
      // analyze type
      if (!macroVal.empty()) {
        if (CToFTypeFormatter::isString(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed " << "h2m" << macroName << "\n";
              LineError(sloc);
            }
            fortranMacro += "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: " + macroName + " = " + macroVal + "\n";
          }
        
        } else if (CToFTypeFormatter::isChar(macroVal)) {
          if (macroName[0] == '_') {
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed " << "h2m" << macroName << "\n";
              LineError(sloc);
            }
            fortranMacro += "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "CHARACTER("+ to_string(macroVal.size()-2)+"), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }
        
        } else if (CToFTypeFormatter::isIntLike(macroVal)) {
          // invalid chars
          if (macroVal.find_first_of("UL") != std::string::npos) {
            if (args.getSilent() == false) {
              errs() << "Warning: Macro with value including UL detected. ";
              errs() << macroName << " Is invalid.\n";
              LineError(sloc);
            }
            fortranMacro = "!INTEGER(C_INT), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          } else if (macroName.front() == '_') {  // Invalid underscore as first character
            if (args.getSilent() == false) {
              errs() << "Warning: Fortran name with invalid characters detected. ";
              errs() << macroName << " renamed h2m" << macroName << "\n"; 
              LineError(sloc);
            }
            fortranMacro = "INTEGER(C_INT), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else if (macroVal.find("x") != std::string::npos) {
            size_t x = macroVal.find_last_of("x");
            string val = macroVal.substr(x+1);
            val.erase(std::remove(val.begin(), val.end(), ')'), val.end());
            val.erase(std::remove(val.begin(), val.end(), '('), val.end());
            fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName + " = int(z\'" + val + "\')\n";
          } else {
            fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }

        } else if (CToFTypeFormatter::isDoubleLike(macroVal)) {
          if (macroVal.find_first_of("FUL") != std::string::npos) {
            if (args.getSilent() == false) {
              errs() << "Warning: macro with value including FUL detected. ";
              errs() << macroName << " Is invalid.\n";
              LineError(sloc);
            }
            fortranMacro = "!REAL(C_DOUBLE), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          } else if (macroName.front() == '_') {
             if (args.getSilent() == false) {
              errs() << "Warning: Fortran names may not start with an underscore. ";
              errs() << macroName << " renamed h2m" << macroName << ".\n";
              LineError(sloc);
            }
            fortranMacro = "REAL(C_DOUBLE), parameter, public :: h2m"+ macroName + " = " + macroVal + "\n";
          } else {
            fortranMacro = "REAL(C_DOUBLE), parameter, public :: "+ macroName + " = " + macroVal + "\n";
          }
          
        } else if (CToFTypeFormatter::isType(macroVal)) {
          // only support int short long char for now
          fortranMacro = CToFTypeFormatter::createFortranType(macroName, macroVal, sloc, args);

        } else {
          std::istringstream in(macroDef);
          for (std::string line; std::getline(in, line);) {
            if (args.getQuiet() == false && args.getSilent() == false) {
              errs() << "Warning: line " << line << " commented out\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        // Check the length of the lines of all object like macros prepared.
        CheckLength(fortranMacro, CToFTypeFormatter::line_max, args.getSilent(), sloc);
    } else { // macroVal.empty(), make the object a bool positive
      if (macroName[0] == '_') {
        if (args.getSilent() == false) {
          errs() << "Warning: Fortran names may not start with an underscore. ";
          errs() << macroName << " renamed h2m" << macroName << ".\n";
          LineError(sloc);
        }
        fortranMacro += "INTEGER(C_INT), parameter, public :: h2m" + macroName  + " = 1 \n";
      } else {
        fortranMacro = "INTEGER(C_INT), parameter, public :: "+ macroName  + " = 1 \n";
      }
      // Check the length of the lines of all the empty macros prepared.
      CheckLength(fortranMacro, CToFTypeFormatter::line_max, args.getSilent(), sloc);
    }


    } else {
        // function macro
      size_t rParen = macroDef.find(')');
      string functionBody = macroDef.substr(rParen+1, macroDef.size()-1);
      if (macroName[0] == '_') {
        fortranMacro += "INTERFACE\n";
        if (args.getSilent() == false) {
          errs() << "Warning: fortran names may not start with an underscore ";
          errs() << macroName << " renamed h2m" << macroName << "\n";
          LineError(sloc);
        }
        if (md->getMacroInfo()->arg_empty()) {
          fortranMacro += "SUBROUTINE h2m" + macroName + "() BIND(C)\n";
        } else {
          fortranMacro += "SUBROUTINE h2m"+ macroName + "(";
          for (auto it = md->getMacroInfo()->arg_begin (); it != md->getMacroInfo()->arg_end (); it++) {
            string argname = (*it)->getName();
            if (argname.front() == '_') {  // Illegal character in argument name
              if (args.getSilent() == false) {
                errs() << "Warning: fortran names may not start with an underscore. Macro argument ";
                errs() << argname << " renamed h2m" << argname << "\n";
                LineError(sloc);
              }
              argname = "h2m" + argname;  // Fix the problem by prepending h2m
            }
            fortranMacro += argname;
            fortranMacro += ", ";
          }
                    // erase the redundant colon
          fortranMacro.erase(fortranMacro.size()-2);
          fortranMacro += ") BIND(C)\n";
          // Check that this line is not too long. Take into account the fact that the
          // characters INTERFACE\n alleady at the begining take up 10 characters and the 
          // newline just added uses another one.
          CheckLength(fortranMacro, CToFTypeFormatter::line_max + 11, args.getSilent(), sloc);

        }
        if (!functionBody.empty()) {  // Comment out the body of the function-like macro 
          std::istringstream in(functionBody);
          for (std::string line; std::getline(in, line);) {
            if (args.getSilent() == false && args.getQuiet() == false) {
              errs() << "Warning: line " << line << " commented out.\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        fortranMacro += "END SUBROUTINE h2m" + macroName + "\n";
        fortranMacro += "END INTERFACE\n";
      } else {
        fortranMacro = "INTERFACE\n";
        if (md->getMacroInfo()->arg_empty()) {
          fortranMacro += "SUBROUTINE "+ macroName + "() BIND(C)\n";
        } else {
          fortranMacro += "SUBROUTINE "+ macroName + "(";
          for (auto it = md->getMacroInfo()->arg_begin (); it != md->getMacroInfo()->arg_end (); it++) {
            string argname = (*it)->getName();
            if (argname.front() == '_') {
              if (args.getSilent() == false) { 
                errs() << "Warning: fortran names may not start with an underscore. Macro argument ";
                errs() << argname << " renamed h2m" << argname << "\n";
                LineError(sloc);
              }
              argname = "h2m" + argname;  // Fix the problem by prepending h2m
            }
            fortranMacro += argname;
            fortranMacro += ", ";
          }
          // erase the redundant colon
          fortranMacro.erase(fortranMacro.size()-2);
          fortranMacro += ") BIND(C)\n";
          // Check that this line is not too long. Take into account the fact that the
          // characters INTERFACE\n alleady at the begining take up 10 characters and the 
          // newline just added uses another one.
          CheckLength(fortranMacro, CToFTypeFormatter::line_max + 11, args.getSilent(), sloc);

        }
        if (!functionBody.empty()) {
          std::istringstream in(functionBody);
          for (std::string line; std::getline(in, line);) {
            if (args.getSilent() == false && args.getQuiet() == false) {
              errs() << "Warning: line " << line << " commented out.\n";
              LineError(sloc);
            }
            fortranMacro += "! " + line + "\n";
          }
        }
        fortranMacro += "END SUBROUTINE " + macroName + "\n";
        fortranMacro += "END INTERFACE\n";
      }
    }

    // Here checks for illegal name lengths and repeated names occur. It seemed best to do this
    // in one place. I must surrender to the strange structure of this function already in place.
    string temp_name;
    if (macroName.front() == '_') {
      temp_name = "h2m" + macroName;
    } else {
      temp_name = macroName;
    }
    // Check the name's length to make sure that it is valid
    CheckLength(temp_name, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Now check to see if this is a repeated identifier. This is very uncommon but could occur.
    if (RecordDeclFormatter::StructAndTypedefGuard(temp_name) == false) {
      if (args.getSilent() == false) {
        errs() << "Warning: skipping duplicate declaration of " << temp_name << ", macro definition.\n";
        LineError(sloc);
      }
      string temp_buf = fortranMacro;  // Comment out all the lines using an istringstream
      fortranMacro = "\n! Duplicate declaration of " + temp_name + ", MACRO, skipped.\n";
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        fortranMacro += "! " + line + "\n";
      }
    }
    

  }
  return fortranMacro;
};


