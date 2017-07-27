PROGRAM test_malloc
USE, intrinsic :: iso_c_binding
USE module_stdlib
USE module_test_malloc

! Set up a structure holding an array.
! This makes access easier.
TYPE, BIND(C) :: memory_bucket
  REAL(C_DOUBLE) :: v(1:50)
END TYPE memory_bucket

! Declare a Fortran pointer to point to the bucket.
TYPE(memory_bucket), POINTER :: my_bucket
! This will be the C location from malloc.
TYPE(C_PTR) my_ptr
! This will be the size to allocate in malloc.
INTEGER(C_SIZE_T) my_size
INTEGER(C_INT) success  ! Used as a function error return value.
INTEGER i  ! Index for the loop

my_size = SIZEOF(my_bucket)
! Allocate enough room for a memory_bucket object.
my_ptr = malloc(my_size)

! Check the association status of the bucket pointer.
if (C_ASSOCIATED(my_ptr)) then
  ! Get a Fortran pointer from the C pointer.
  Call C_F_Pointer(my_ptr, my_bucket)
  ! Check that the conversion was successful.
  if (ASSOCIATED(my_bucket)) then
    ! Fill in the bucket's values.
    do i = 1,50 
      my_bucket%v(i) = i
      WRITE(*,*) my_bucket%v(i)
    end do
  endif
  WRITE(*,*) "is the orginal array"

  ! Call a simple C routine to shuffle
  ! the array.
  CALL shuffle_nums(my_bucket%v)

  ! Write out the shuffled bucket values.
  do i = 1,50
    WRITE(*,*) my_bucket%v(i)
  end do
  WRITE(*,*) "is the shuffled array"

  ! Nullify the pointers and free memory.
  nullify(my_bucket)
  CALL free(my_ptr)
endif


END PROGRAM test_malloc
