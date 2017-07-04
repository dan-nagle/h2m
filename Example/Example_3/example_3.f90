PROGRAM example_3
USE, INTRINSIC :: iso_c_binding
USE module_example_3
implicit none

CALL initialize_vars

WRITE(*,*) "Our character is: "
WRITE(*,*) my_global_char
WRITE(*,*) "Our global double is: "
WRITE(*,*) my_global_double

END PROGRAM example_3
