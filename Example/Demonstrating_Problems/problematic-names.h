// C is more permissive about naming than Fortran.
// In C, capitalization differentiates identifiers but
// this is not the case in Fortran. Problems related
// to C file-scope (static storage) may also arise.
// Note that Fortran identifiers cannot start with 
// an underscore, but C names can. 
// Because h2m uses the filename to create the module
// name, if the filename has an illegal character (for
// this example, a - in it, an illegal module name will
// be created.

int _my_underscored_integer;

void _my_underscored_subroutine();

struct _underscored_struct {
  double _underscored_double;
};

// To fix the problem with the leading underscore, which
// h2m checks for in every piece of code that might
// appear in a module, h2m prepends illegal names with
// "h2m".
// The resulting linking issues can sometimes be resolved
// with the -autobind or -b option by h2m. Otherwise, the
// names of the C variables will have to be altered. 
