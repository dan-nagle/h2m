PROGRAM example
USE, INTRINSIC :: iso_c_binding
USE module_example
  REAL(C_DOUBLE) :: result
  TYPE(circle) my_shape
  my_shape%x = 2
  my_shape%y = 3
  my_shape%radius = 2.5
  
  result = calculate_area(my_shape)
  CALL describe_circle(my_shape)


END PROGRAM example
