// Here are the definition of formatter classes for the h2m
// small formatter classes, including RecordDeclFormatter,
// TypedefDeclFomatter and EnumDeclFormatter.

#include "h2m.h"

// This function exists to make sure that a type is not declared
// twice. This frequently happens with typedefs renaming structs.
// This results in duplicate-name-declaration errors in Fortran
// because there are no seperate name-look-up procedures for
// "tag-names" as there are in C (ie in C "typedef struct Point point"
// is legal but in Fortran this causes conflicts.) A static map will
// make sure that no typedef declares an already declared name.
// If the name has already been seen, it returns false. If it hasn't,
// it adds it to a set (will return false if called again with that
// name) and returns true. It will also return true if the name is "".
// This is assumed to be a mistake of some kind.
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
  // Initialize the object's status variables.
  current_status = CToFTypeFormatter::OKAY;
  error_string = "";
  isLocValid = typedefDecl->getSourceRange().getBegin().isValid();
  // sloc, if uninitialized, will be an invalid location. Because it 
  // is always passed to a function which checks validity, this should 
  // be a fine way to guard against an invalid location.
  if (isLocValid) {
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        typedefDecl->getSourceRange().getBegin());
    sloc = rewriter.getSourceMgr().getPresumedLoc(
        typedefDecl->getSourceRange().getBegin());
  }  
  
};

// From a C typedef, a string representing a Fortran pseudo-typedef
// is created. The fortran equivalent is a type with only one field. 
// The name of this field is name_type (ie name_C_INT), depending 
// on the type. A typedef with an illegal name 
// will be prepended with "h2m" to become legal fortran. This function 
// will check for name duplication. In the case that this is a
// duplicate identifier, the status will be set appropriately to
// CToFTypeFormatter::DUPLICATE. Note that no bindname is allowed
// because BIND(C, name="") is not permitted in a TYPE.
string TypedefDeclFormater::getFortranTypedefDeclASString() {
  string typedef_buffer = "";
  if (isLocValid && !isInSystemHeader) {  // Keeps system files from leaking in
    // We fetch the typedef information from the AST to begin work.
    TypeSourceInfo * typeSourceInfo = typedefDecl->getTypeSourceInfo();
    CToFTypeFormatter tf(typeSourceInfo->getType(), typedefDecl->getASTContext(),
        sloc, args);
    string identifier = typedefDecl->getNameAsString();
    if (identifier.front() == '_') {  // This identifier has an illegal _ at the begining.
      CToFTypeFormatter::PrependError(identifier, args, sloc);
      identifier = "h2m" + identifier;  // Prepend h2m to fix the problem.
    }
    
    // Include the bindname, which may be empty, when assembling the definition.
    typedef_buffer = "TYPE, BIND(C) :: " + identifier + "\n";
    // Because names in typedefs may collide with the typedef name, 
    // suffixes are appended to the internal member of the typedef.
    // This flag will indicate the presence of an unrecognized type.
    bool problem;
    string type_wrapper_name = tf.getFortranTypeASString(true, problem);
    string type_no_wrapper = tf.getFortranTypeASString(false, problem); 
    if (problem == true) {  // Set status to reflect failure.
      current_status = CToFTypeFormatter::BAD_TYPE;
      error_string = type_wrapper_name;
    }
    // Give a special warning about the odd way in which typedefs are made.
    if (args.getSilent() == false) {
      errs() << "Warning: due to name collisions during typdef translation, " <<
          identifier;
      errs() <<  "\nrenamed " << identifier << "_" << type_no_wrapper << "\n";
      CToFTypeFormatter::LineError(sloc);
    }
    string modified_name = identifier + "_" + type_no_wrapper;
    string to_add = "    " + type_wrapper_name + "::" + modified_name + "\n";
    // Set the flag to reflect a bad name length if necessary
    if (modified_name.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = modified_name;
    }
    // Check for an illegal line length. Note that we add one to account for
    // the newline character which shouldn't cound against line length.
    if (to_add.length() > CToFTypeFormatter::line_max + 1) {
      current_status = CToFTypeFormatter::BAD_LINE_LENGTH;
      error_string = to_add;
    }
    typedef_buffer += to_add;
    typedef_buffer += "END TYPE " + identifier + "\n";
    // Check to see whether we have declared something with this identifier before.
    bool not_repeat = RecordDeclFormatter::StructAndTypedefGuard(identifier); 
    if (not_repeat == false) {  // This indicates that this is a duplicate identifier.
      current_status = CToFTypeFormatter::DUPLICATE;
      error_string = identifier + ", TYPEDEF.";
      
    }
  } 
  return typedef_buffer;
};

// -----------initializer EnumDeclFormatter--------------------
EnumDeclFormatter::EnumDeclFormatter(EnumDecl *e, Rewriter &r, 
    Arguments &arg) : rewriter(r), args(arg) {
  enumDecl = e;
  current_status = CToFTypeFormatter::OKAY;
  error_string = "";
  // Becasue sloc is only ever passed to a function which checks its validity,
  // this should be a fine/ way to deal with an invalid location. An empty
  //  sloc is an invalid location.
  if (enumDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(
        enumDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        enumDecl->getSourceRange().getBegin());
  } else {
    isInSystemHeader = false;  // If it isn't anywhere, it isn't in a system header
  }
};

// From a C enumerated type, a fotran ENUM is created. The names are prepended
// with h2m if they begin with an underscore. The function loops through all 
// the members in the enumerator and adds them into place. Note that therei
// is no option to use a bind name here because it is not permitted to have
// a BIND(C, name="") statement in an Enum.
string EnumDeclFormatter::getFortranEnumASString() {
  string enum_buffer;
  bool anon = false;  // The flag for an anonymous type.
  // This is a hold over from an old function structure 
  // and really should be made more elegant -Michelle

   // Keeps definitions in system headers from leaking into the translation.
   if (!isInSystemHeader) { 
    string enumName = enumDecl->getNameAsString();

    // We don't have a proper name. We must get another form of identifier.
    if (enumName.empty() == true) {
      enumName = enumDecl->getTypeForDecl()->
          getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // This checks to make sure that this is not an anonymous enumeration
      // This is not a problem, so we don't set a bad status, we just
      // deal with names slightly differently.
      if (enumName.find("anonymous at") != string::npos) {
        anon = true;  // Sets a bool to let us know that we have no name.
      }
    }

    if (enumName.front() == '_') {  // Illegal underscore beginning the name!
      CToFTypeFormatter::PrependError(enumName, args, sloc);
      enumName = "h2m" + enumName;  // Prepend h2m to fix the problem
    }

    // If there is no name, we don't add one on to the 
    // definition. Note that enums don't have "real" names
    // in Fortran. The name is commented out.
    if (anon == false) {
      enum_buffer = "ENUM, BIND(C) ! " + enumName + "\n";
    } else {  // Handle a nameless enum as best we can.
      enum_buffer = "ENUM, BIND(C)\n";
    }

    // Cycle through the pieces of the enum and translate them into fortran.
    for (auto it = enumDecl->enumerator_begin (); it != enumDecl->enumerator_end(); it++) {
      string constName = (*it)->getNameAsString ();
      if (constName.front() == '_') {  // The name begins with an illegal underscore.
        CToFTypeFormatter::PrependError(constName, args, sloc);
        constName = "h2m" + constName;
      }
      int constVal = (*it)->getInitVal().getExtValue();  // Get the initialization value
      // Check for a valid name length. Note that the line can't be too 
      // long unless the name is hopelessly too long so we only 
      // chech for a name being too long. Set the status if
      // necessary.
      if (constName.length() > CToFTypeFormatter::name_max) {
        current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
        error_string = constName + ", ENUM member.";
      }
      // If there is a duplicate identifier, set the flag to reflect
      // the problem.
      if (RecordDeclFormatter::StructAndTypedefGuard(constName) == false) { 
        current_status = CToFTypeFormatter::DUPLICATE;
        error_string = constName + ", ENUM member.";
      }
      enum_buffer += "ENUMERATOR :: " + constName + " = " + 
          std::to_string(constVal) + "\n";
    }

    // Because the actual enum name is commented out, we don't check it for a repeat. 
    if (anon == false) {  // Put the name after END ENUM unless there is no name.
      enum_buffer += "END ENUM !" + enumName + "\n";
    } else {
      enum_buffer += "END ENUM\n";
    }
  }

  return enum_buffer;
};

// -----------initializer RecordDeclFormatter--------------------

RecordDeclFormatter::RecordDeclFormatter(RecordDecl* rd, Rewriter &r, 
    Arguments &arg) : rewriter(r), args(arg) {
  recordDecl = rd;
  error_string = "";
  current_status = CToFTypeFormatter::OKAY;
  // Because sloc is checked for validity prior to use, this should be a
  // fine way to deal with an invalid source location
  if (recordDecl->getSourceRange().getBegin().isValid()) {
    sloc = rewriter.getSourceMgr().getPresumedLoc(
        recordDecl->getSourceRange().getBegin());
    isInSystemHeader = rewriter.getSourceMgr().isInSystemHeader(
        recordDecl->getSourceRange().getBegin());
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
// stores (essentially) in a seperate symbol table. Structs are usually
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
    int iterations = 0;  // Used to come up with a name for empty fields.
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); it++) {
      CToFTypeFormatter tf((*it)->getType(), recordDecl->getASTContext(), sloc, args);
      string identifier = tf.getFortranIdASString((*it)->getNameAsString());
      if (identifier.empty() == true) {  // There is no identifier. We make one.
        identifier = "field_" + std::to_string(iterations);
      } 
      if (identifier.front() == '_') {  // There is an illegal identifier.
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        identifier = "h2m" + identifier;
      }
      // The helper function will set this error flag if it finds an 
      // illegal type of any kind.
      bool problem = false;
      fieldsInFortran += "    " + tf.getFortranTypeASString(true, problem) +
          " :: " + identifier + "\n";
      if (problem == true) {  // We have encountered an illegal type
        current_status = CToFTypeFormatter::BAD_TYPE;
        error_string = tf.getFortranTypeASString(true, problem);
      }
      ++iterations;
    }
  }
  return fieldsInFortran;
}

// The procedure for any of the four sorts of structs/typedefs is fairly
// similar, but anonymous structs need to be handled specially. This 
// function puts together the name of the struct as well as the fields fetched
// from the getFortranFields() function above. All illegal names are prepended
// with h2m. Checks are made for duplicate names. Note that no option for a 
// bindname is allowed because BIND(C, name="") statements are illegal in
// a TYPE definition.
string RecordDeclFormatter::getFortranStructASString() {
  // Initialize the mode (ie is this a typedef, tag struct, ID struct, anonymous?)
  setMode();
  string identifier = "";  // Holds the Fortran name for this structure.
  string rd_buffer;  // Holds the entire declaration.

  if (!isInSystemHeader) {  // Prevents system headers from leaking in to the file
    string fieldsInFortran = getFortranFields();  // Get the struct's pieces.
    if (fieldsInFortran.empty()) {  // Warn about an empty struct.
      rd_buffer = "! struct without fields may cause warnings\n";
      if (args.getSilent() == false && args.getQuiet() == false) {
        errs() << "Warning: struct without fields may cause warnings: \n";
        CToFTypeFormatter::LineError(sloc);
      }
    }
   
    // Different modes require slightly different treatments to get the 
    // proper name.
    if (mode == ID_ONLY) {
      identifier = recordDecl->getNameAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }

      // Declare the structure in Fortran.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == TAG_ONLY) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    } else if (mode == ID_TAG) {
      identifier = tag_name;
      if (identifier.front() == '_') {  // Illegal underscore detected
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }

      // Assemble the strucutre in Fortran. Note that bindname may be empty.
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";    
    // This should never be called. Typedefs should be processed in their
    // own places. This is for insurance.
    } else if (mode == TYPEDEF) {
      identifier = recordDecl->getTypeForDecl(
          )->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      if (identifier.front() == '_') {  // Illegal underscore detected
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }

      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";
    // An anonymous strut may or may not have a declared name.
    } else if (mode == ANONYMOUS) {
      identifier = recordDecl->getTypeForDecl()->
          getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
      // Erase past all the spaces so that only the name remains (ie get 
      // rid of the "struct" part)
      size_t found = identifier.find_first_of(" ");
      while (found!=string::npos) {
        identifier.erase(0, found + 1);
        found=identifier.find_first_of(" ");
      }
      if (identifier.front() == '_') {  // Illegal underscore detected
        CToFTypeFormatter::PrependError(identifier, args, sloc);
        identifier = "h2m" + identifier;  // Fix the problem by prepending h2m
      }
      current_status = CToFTypeFormatter::BAD_ANON;
      error_string = identifier + ", anonymous struct.";
      rd_buffer += "TYPE, BIND(C) :: " + identifier + "\n";
      rd_buffer += fieldsInFortran + "END TYPE " + identifier +"\n";

    }
    // Now that we are through processing structs of all types, we must check for the problems
    // require us to comment the struct out: duplicate names or invalid types.
    // The lines that make up the struct were checked for length earlier, and a struct's
    // first line cannot be too long unless the identifier is hopelessly too long.

    // Check to see whether we have declared something with this identifier before.
    bool not_repeat = RecordDeclFormatter::StructAndTypedefGuard(identifier); 
    if (not_repeat == false) {  // This indicates a duplicate
      current_status = CToFTypeFormatter::DUPLICATE;
      error_string = identifier + ", structured type.";
    }
    // Check for a name which is too long. 
    if (identifier.length() > CToFTypeFormatter::name_max) {
      current_status = CToFTypeFormatter::BAD_NAME_LENGTH;
      error_string = identifier;
    }
  }
  return rd_buffer;    
};

// Determines what sort of struct we are dealing with. The differences seem
// subtle to me. If you need to understand what these modes are, you will
// have to play around with some structs. -Michelle
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
    string identifier = recordDecl->getTypeForDecl()
        ->getLocallyUnqualifiedSingleStepDesugaredType().getAsString();
    if (identifier.find(" ") != string::npos) {
      mode = ANONYMOUS;
    } else {
      mode = TYPEDEF;
    }
    
  }
};

