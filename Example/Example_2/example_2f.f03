! The following program is part of a contrived example to
! demonstrate the use of C function pointers in a Fortran
! program. Note that, in order to call the function pointer,
! a subroutine included in the intrinsic iso_c_binding module
! must be called. C_F_PROCPOINTER is the procedure used to
! transform a C function pointer into a Fortran function or
! procedure which can be called by Fortran.

PROGRAM example_2
USE, INTRINSIC :: iso_c_binding
USE module_example_2

! The ABSTRACT INTERFACE is part of the Fortran 2003 standard.
! It is the recommended way to give a type to a Fortran procedure
! pointer which might potentially point to several different
! C functions.
ABSTRACT INTERFACE
  CHARACTER(C_CHAR) FUNCTION c_func_type(code) bind (C)
    USE iso_c_binding, only: C_INT, C_CHAR
    import
    INTEGER(C_INT), value :: code
  END FUNCTION
END INTERFACE

! Declare the variables to pass into and obtain from 
! C functions.
PROCEDURE(c_func_type), POINTER :: fort_fun_ptr
TYPE(C_FUNPTR) :: c_fun_ptr
REAL(C_DOUBLE) :: first = 5
REAL(C_DOUBLE) :: second = 200
CHARACTER(C_CHAR) :: operation = '\'
CHARACTER(C_CHAR) :: result_char
INTEGER(C_INT) :: the_code = -1

! Obtain the function pointer from the C function.
c_fun_ptr = return_f_ptr(first, second, operation)

! Convert the C function pointer into something that
! Fortran can call.
CALL C_F_PROCPOINTER(c_fun_ptr, fort_fun_ptr)

! Call the C function through the Fortran translation.
result_char = fort_fun_ptr(the_code)

! Write the result to standard output.
WRITE(*,*) result_char

END PROGRAM example_2
