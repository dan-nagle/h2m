! This program writes a data set of six numbers into 
! a netcdf file.
PROGRAM simple_ncdf_write
USE, intrinsic :: iso_c_binding
USE module_netcdf
! Set up a grid for the data.
integer(C_INT), parameter :: dims = 2, dimx = 3, dimy = 6
! This is the file name.
character(kind=C_CHAR, len=20), target :: FILE_NAME = "simple_write.nc" //C_NULL_CHAR
! This is the name of the data set
character(kind=C_CHAR, len=6), target :: data_name = "data" //C_NULL_CHAR
! The identifiers needed for the operations and the function return value
integer(C_INT), target :: ncid, varid, dimids(dims), x_dimid, y_dimid, error
! These are the names of the dimensions.
character(kind=C_CHAR, len = 2), target :: x = "x" //C_NULL_CHAR
character(kind=C_CHAR, len = 2), target :: y = "y" //C_NULL_CHAR
! These are the actual dimensions of the grid to be created.
integer(C_SIZE_T), parameter :: numx = 2, numy =3

! This is the data to write
integer(C_INT), target :: stuff_to_write(dimx, dimy)

! This is for loop indexing
integer(C_INT) :: index1, index2

! We make fake data
do index1 = 1, dimx
  do index2 = 1, dimy
    stuff_to_write(index1, index2) = index1 + index2
  end do
end do

! Create a new netcdf file
error = nc_create(C_LOC(FILE_NAME), NC_CLOBBER, C_LOC(ncid))
WRITE(*,*) error

! Define the dimensions fof the file.
error = nc_def_dim(ncid, C_LOC(x), numx, C_LOC(x_dimid))
WRITE(*,*) error
error = nc_def_dim(ncid, C_LOC(y), numy, C_LOC(y_dimid))
WRITE(*,*) error

! Put the dimension ID's in an array.
dimids(1) = x_dimid
dimids(2) = y_dimid
! Define the variable to be written into the file.
error = nc_def_var(ncid, C_LOC(data_name), NC_INT, dims, C_LOC(dimids), C_LOC(varid))

WRITE(*,*) error
! End the definition of the variable
error = nc_enddef(ncid)
WRITE(*,*) error
! Write integer values into the grid
error = nc_put_var_int(ncid, varid, C_LOC(stuff_to_write))
WRITE(*,*) error

! Close the netcdf file
error = nc_close(ncid)
WRITE(*,*) error
end program simple_ncdf_write
