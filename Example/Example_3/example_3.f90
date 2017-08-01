! This is the Fortran driver program which 
! calls the C defined initialization routine
! and then prints out the values of the variables
! as defined in C.

PROGRAM example_3
USE, INTRINSIC :: iso_c_binding
USE module_example_3
implicit none

INTEGER i, j ! Loop counters

h2m_my_global_other = 9 ! This is set to zero in C

! Initialize the variables in C.
CALL initialize_vars

WRITE(*,*) "Our character is: "
WRITE(*,*) my_global_char
WRITE(*,*) "Our global double is: "
WRITE(*,*) my_global_double
WRITE(*,*) "Our _my_global_other/h2m_my_global_other is: "
WRITE(*,*) h2m_my_global_other

! Print out the small array (which is twelve numbers long)
WRITE(*,*) "Our array is: "
do i = 1, 3
  do j = 1, 4
    WRITE(*,*) our_global_numbers(j, i)
  end do
end do

END PROGRAM example_3
