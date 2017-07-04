! This is the Fortran driver program which 
! calls the C defined initialization routine
! and then prints out the values of the variables
! as defined in C.

PROGRAM example_3
USE, INTRINSIC :: iso_c_binding
USE module_example_3
implicit none

INTEGER i

! Initialize the variables
CALL initialize_vars

WRITE(*,*) "Our character is: "
WRITE(*,*) my_global_char
WRITE(*,*) "Our global double is: "
WRITE(*,*) my_global_double

! Print out the small array (which is four long)
WRITE(*,*) "Our array is: "
do i = 1, 4
  WRITE(*,*) our_global_numbers(i)
end do

END PROGRAM example_3
