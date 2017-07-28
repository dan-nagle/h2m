! This program reads from the netcdf file
! containing six integers written by 
! the ncdf_write.f90 program.
PROGRAM read
USE, intrinsic :: iso_c_binding
USE module_netcdf
integer(C_INT), parameter :: nx = 3, ny = 6 ! Dimensions
character(len=20, kind=c_char), target :: FILE_NAME = "simple_write.nc" //C_NULL_CHAR
! Name of the variable to read.
character(len=10, kind=c_char), target :: data_name = "data" //C_NULL_CHAR
! Identification number and variable identifer
integer(C_INT), target ::  ncid, varid
! Function return value
integer(C_INT) error
! Array into which to read data
integer(C_INT), target :: data_in(nx, ny)
! Loop indexes
integer :: index1, index2

! Open the netcdf file
error = nc_open(C_LOC(FILE_NAME), NC_NOWRITE, C_LOC(ncid))
! Report any errors

! Get the identifiers from the data name.
error = nc_inq_varid(ncid, C_LOC(data_name), C_LOC(data_in))

! Get the data from the file.
error = nc_get_var_int(ncid, varid, C_LOC(data_in))

! Loop through and print the information
do index1 = 1, nx
  do index2 = 1, ny
      WRITE(*,*) data_in(index1, index2)
  end do
end do

! Close the netcdf file.
error = nc_close(ncid)

end program nc_read
