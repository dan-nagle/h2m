// Here are the definition of formatter classes for the h2m
// small formatter classes, including RecordDeclFormatter,
// TypedefDeclFomatter and EnumDeclFormatter.

#include "h2m.h"

// This function exists to make sure that a type is not declared
// twice. This frequently happens with typedefs renaming structs.
// This results in duplicate-name-declaration errors in Fortran
// because there are no seperate name-look-up procedures for
// "tag-names" as there are in C (ie in C typedef struct Point point
// is legal but in Fortran this causes conflicts.) A static map will
// make sure that no typedef declares an already declared name.
// If the name has already been seen, it returns false. If it hasn't,
// it adds it to a set (will return false if called again with that
// name) and returns true. It will also return true if the name is ""
bool RecordDeclFormatter::StructAndTypedefGuard(string name) {
  static std::set<string> seennames;  // Records all identifiers seen.
  // This is insurance against accidental improper calling of
  // this function. No name is actually empty, so this can't be a
  // repeated name.
  if (name.compare("") == 0) {
    return true;
  }
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

// -----------initializer Typedef--------------------
TypedefDeclFormater::TypedefDeclFormater(TypedefDecl *t, Rewriter &r,
    Arguments &arg) : rewriter(r), args(arg) {
  typedefDecl = t;
  Okay = true;
  isLocValid = typedefDecl->getSourceRange().getBegin().isValid();
  // sloc, if uninitialized, will be an invalid location. Because it 
  // is always passed to a function which checks validity, this should 
  // be a fine way to guard against an invalid location.
  if (isLocValid) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        typedefDecl->getSourceRange().getBegin());
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
  string typedef_buffer = "";
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
        CToFTypeFormatter::LineError(sloc);
      }
      identifier = "h2m" + identifier;  // Prepen dh2m to fix the problem.
    }
    

    // Check to make sure the identifier is not too lone
    CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Include the bindname, which may be empty, when assembling the definition.
    typedef_buffer = "TYPE, BIND(C) :: " + identifier + "\n";
    // Because names in typedefs may collide with the typedef name, 
    // suffixes are appended to the internal member of the typedef.
    bool problem;
    string type_wrapper_name = tf.getFortranTypeASString(true, problem);
    string type_no_wrapper = tf.getFortranTypeASString(false, problem); 
    if (problem == true) {
      Okay = false;
    }
    if (args.getSilent() == false) {
      errs() << "Warning: due to name collisions during typdef translation, " << identifier;
      errs() <<  "\nrenamed " << identifier << "_" << type_no_wrapper << "\n";
      CToFTypeFormatter::LineError(sloc);
    }
    string to_add = "    "+ type_wrapper_name + "::" + identifier+"_"+ type_no_wrapper + "\n";
    CToFTypeFormatter::CheckLength(identifier + "_" + type_no_wrapper,
        CToFTypeFormatter::name_max, args.getSilent(), sloc);
    // Check for an illegal length. The \n character is the reason for the +1. It doesn't count
    // towards line length.
    CToFTypeFormatter::CheckLength(to_add, CToFTypeFormatter::line_max + 1, args.getSilent(), sloc);
    typedef_buffer += to_add;
    typedef_buffer += "END TYPE " + identifier + "\n";
  //  }
    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary. Also skip the declaration if
    // Okay is false and we have been asked to comment out invalid declarations
    bool repeat = RecordDeclFormatter::StructAndTypedefGuard(identifier); 
    if (repeat == false || (Okay == false && args.getDetectInvalid() == true)) {
      string warning = "";
      string intext = "";  // This is the intext warning for the problem
      // Determine the explanatory warning to emit and emit it unless silenced
      if (repeat == false) {
        warning = "Warning: skipping duplicate declaration of " + identifier + "\n";
        intext = "\n! Duplicate declaration of " + identifier + ", TYPEDEF, skipped. \n";
      } else {
        warning = "Warning: illegal type in " + identifier + "\n";
        intext = "\n! Illegal type in " + identifier + " TYPEDEF.\n";
      }
      if (args.getSilent() == false) {
        errs() << warning;
        CToFTypeFormatter::LineError(sloc);
      }
      string temp_buf = typedef_buffer;
      // Reset the buffer to hold only the in text error warning.
      typedef_buffer = intext;
      std::istringstream in(temp_buf);
      // Loops through the buffer like a line-buffered stream and comments it out
      for (std::string line; std::getline(in, line);) {
        typedef_buffer += "! " + line + "\n";
      }
      
    }
  } 
  return typedef_buffer;
};

// -----------initializer EnumDeclFormatter--------------------
EnumDeclFormatter::EnumDeclFormatter(EnumDecl *e, Rewriter &r, Arguments &arg) : rewriter(r), args(arg) {
  enumDecl = e;
  Okay = true;
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
        CToFTypeFormatter::LineError(sloc); 
      } 
      enumName = "h2m" + enumName;  // Prepend h2m to fix the problem
    }

    // Check the length of the name to make sure it is valid Fortran
    // Note that it would be impossible for this line to be an illegal length unless
    // the variable name were hopelessly over the length limit. Note bindname may be empty.
    CToFTypeFormatter::CheckLength(enumName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
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
          CToFTypeFormatter::LineError(sloc);
        }
      }
      int constVal = (*it)->getInitVal().getExtValue();  // Get the initialization value
      // Check for a valid name length
      CToFTypeFormatter::CheckLength(constName, CToFTypeFormatter::name_max, args.getSilent(), sloc);
      // Problem! We have seen an identifier with this name before! Comment out the line
      // and warn about it.
      if (RecordDeclFormatter::StructAndTypedefGuard(constName) == false) { 
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << constName << ", enum member.\n";
          CToFTypeFormatter::LineError(sloc);
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
    // Skip this duplicate declaration if necessary. With the current set up, this
    // isn't actually needed because the enum name is commented out, but the second
    // check (of Okay's status) is very important and this is here for consistency
    bool name_guard = RecordDeclFormatter::StructAndTypedefGuard(enumName);
    if (name_guard  == false || (Okay == false &&
        args.getDetectInvalid() == true)) {
      string warning = "";
      string intext = "";  // This will be the in text error warning.
      // Determine the appropriate warning to print about why we are commenting
      // out this section and emit it unless silenced.
      if (name_guard == false) {
        warning = "Warning: skipping duplicate declaration of " + enumName + "\n";
        intext = "\n! Duplicate declaration of " + enumName + ", ENUM, skipped.\n";
      } else {
        warning = "Warning: illegal type in " + enumName + "\n";  
        intext = "\n! Invalid type in " + enumName + ", ENUM.\n"; 
      }
      if (args.getSilent() == false) {
        errs() << warning;
        CToFTypeFormatter::LineError(sloc);
      }

      // Comment out the declaration by stepping through and appending ! before newlines
      // to avoid duplicate identifier collisions.
      string temp_buf = enum_buffer;
      // Add an explanation in the text for commenting it out
      enum_buffer = intext;
      std::istringstream in(temp_buf);
      for (std::string line; std::getline(in, line);) {
        enum_buffer += "! " + line + "\n";
      }
      // Add in a few last details... and return to avoid having END ENUM pasted on the end
      // an anonymous enum doesn't need a name added in
      if (anon == false) {
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
  Okay = true;
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
    // Guard against an empty declaration and iterate through the fields
    // in the structured type, checking for invalid names.
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); it++) {
      CToFTypeFormatter tf((*it)->getType(), recordDecl->getASTContext(), sloc, args);
      string identifier = tf.getFortranIdASString((*it)->getNameAsString());
      if (identifier.front() == '_') {
        if (args.getSilent() == false) {
          errs() << "Warning: invalid struct field name " << identifier;
          errs() << " renamed h2m" << identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;
      }
      // Make sure that the field's identifier isn't too long for a fortran name
      CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);

      bool problem = false;
      fieldsInFortran += "    " + tf.getFortranTypeASString(true, problem) +
          " :: " + identifier + "\n";
      if (problem == true) {  // Set the object's error flag
        Okay = false;
      }
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
  Okay = true;
  string identifier = "";  // Holds the Fortran name for this structure.
  // A value of TRUE indicates that this declaration is okay. A value
  // of false indicates that this is a repeated name and needs to be
  // commented out.

  string rd_buffer;  // Holds the entire declaration.

  if (!isInSystemHeader) {  // Prevents system headers from leaking in to the file
    string fieldsInFortran = getFortranFields();
    if (fieldsInFortran.empty()) {
      rd_buffer = "! struct without fields may cause warnings\n";
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Warning: struct without fields may cause warnings: \n";
        CToFTypeFormatter::LineError(sloc);
      }
    }

    if (mode == ID_ONLY) {
      identifier = recordDecl->getNameAsString();
            if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, args.getSilent(), sloc);

      // Declare the structure in Fortran. The bindname may be empty.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == TAG_ONLY) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" <<
              identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max,
          args.getSilent(), sloc);

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ID_TAG) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid structure name " << identifier << " renamed h2m" << identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, 
          args.getSilent(), sloc);

      // Assemble the strucutre in Fortran. Note that bindname may be empty.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";    
    } else if (mode == TYPEDEF) {
      identifier = recordDecl->getTypeForDecl(
          )->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        if (args.getSilent() == false) {
          errs() << "Warning: invalid typedef name " << identifier << 
              " renamed h2m" << identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // Check for a name which is too long. Note that if the name isn't hopelessly too long, the
      // line can be guaranteed not to be too long.
      CToFTypeFormatter::CheckLength(identifier, CToFTypeFormatter::name_max, 
          args.getSilent(), sloc);

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
          CToFTypeFormatter::LineError(sloc);
        }
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      // We have previously seen a declaration with this name. This shouldn't be possible, so don't 
      // worry about commenting it out. It's commented out anyway.
      if (RecordDeclFormatter::StructAndTypedefGuard(identifier) == false) {
        if (args.getSilent() == false) {
          errs() << "Warning: skipping duplicate declaration of " << identifier << "\n";
          CToFTypeFormatter::LineError(sloc);
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
          CToFTypeFormatter::LineError(sloc);
        }
      }
      // Avoid any potential double-commenting problems below by returning the
      // already commented out buffer now.
      return rd_buffer;  
    }
    // Now that we are through processing structs of all types, we must check for the problems
    // require us to comment the struct out: duplicate names or invalid types.

    // Check to see whether we have declared something with this identifier before.
    // Skip this duplicate declaration if necessary. Also skip if it has an invalid
    // type in it and we've been asked to comment those out.
    bool repeat = RecordDeclFormatter::StructAndTypedefGuard(identifier); 
    if (repeat == false || (Okay == false && args.getDetectInvalid() == true)) {
      // Determine and emit the proper warning for the circumstances.
      string warning;
      string intext;  // This will be the warning in the translated code
      if (repeat == false) {
        warning = "Warning: skipping duplicate declaration of " + identifier + "\n";
        intext = "\n! Duplicate declaration of " + identifier +
            ", structured type, skipped.\n";
      } else {
        warning = "Warning: invalid type in " + identifier + "\n";
        intext = "\n! Invalid type in " + identifier + ", STRUCT.\n";
      }
      if (args.getSilent() == false) {
        errs() << warning;
        CToFTypeFormatter::LineError(sloc);
      }
      // Comment out the declaration by stepping through and appending ! before newlines
      // Also put in the correct explanation for the commented section
      std::istringstream in(rd_buffer);
      rd_buffer = intext;  // Put the intext warning in first.
      for (std::string line; std::getline(in, line);) {
        rd_buffer += "! " + line + "\n";
      }
    }
  }
  return rd_buffer;    
};

// Determines what sort of struct we are dealing with. The differences seem
// subtle to me. If you need to understand what these modes are, you will
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

