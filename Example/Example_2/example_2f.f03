PROGRAM example_2
USE, INTRINSIC :: iso_c_binding
USE module_example_2

ABSTRACT INTERFACE
  CHARACTER(C_CHAR) FUNCTION c_func_type(code) bind (C)
    USE iso_c_binding, only :: C_INT, C_CHAR
    import
    INTEGER(C_INT), value :: code
  END FUNCTION
END INTERFACE

PROCEDURE(c_func_type), POINTER :: fort_fun_ptr
TYPE(C_FUNPTR) :: c_fun_ptr
REAL(C_DOUBLE) :: first = 5
REAL(C_DOUBLE) :: second = 200
CHARACTER(C_CHAR) :: operation = '\'
CHARACTER(C_CHAR) :: result_char
INTEGER(C_INT) :: the_code = -1

return_f_ptr(first, second, operation)

CALL C_F_PROCPOINTER(c_fun_ptr, fort_fun_ptr)

result_char = fort_fun_ptr(the_code)

WRITE(*,*) reult_char

END PROGRAM example_2
