! This program must be linked to the curl
! library and to the C object file where
! the C function data_write is compiled.
! This requires a compilation line something
! like:
! gfortran curl_download.f90 write_c.o -lcurl.

PROGRAM use_curl
use, intrinsic :: iso_c_binding
use module_curl
use module_stdio_2
use module_c_writer_helper
implicit none

integer(c_long) :: CURL_GLOBAL_ALL = 3
integer(c_int) errors
TYPE(C_PTR), TARGET :: handle, file_handle
CHARACTER(Kind=C_CHAR, len=25), TARGET :: url, filename
CHARACTER(Kind=C_CHAR, len=5), TARGET :: mode

errors = curl_global_init(CURL_GLOBAL_ALL)

! Let's get ourselves a handle!
handle = curl_easy_init()
if (.NOT. C_ASSOCIATED(handle)) then
  WRITE(*,*) "You have no pointer."
  stop 1
endif
! Attempt to set the URL to google.
url = "http://www.google.com" // C_NULL_CHAR
mode = "wb" // C_NULL_CHAR  ! Mode to open file.
! Send data to this location
filename = "web_page.txt" //C_NULL_CHAR

file_handle = fopen(C_LOC(filename), C_LOC(mode))
if (.NOT. C_ASSOCIATED(file_handle)) then
  WRITE(*,*) "Error getting file handle."
  stop 1
endif

errors = curl_easy_setopt(handle, CURLOPT_URL, C_LOC(url))
if (errors .NE. 0) then
  WRITE(*,*) "Error for you sir:", errors
  stop 1
endif

! Set the writer function
errors = curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, C_FUNLOC(data_write))
if (errors .NE. 0) then
  WRITE(*,*) "Error for you sir:", errors
  stop 1
endif

! Tell curl to write into this file.
errors = curl_easy_setopt(handle, CURLOPT_WRITEDATA, file_handle)
if (errors .NE. 0) then
  WRITE(*,*) "Error for you sir:", errors
  stop 1
endif

! Attempt to do the curl thing.
errors = curl_easy_perform(handle)
if (errors .NE. 0) then
  WRITE(*,*) curl_easy_strerror(errors)
  stop 1
endif
! Close the file
errors = fclose(file_handle)
if (errors .NE. 0) then
  WRITE(*,*) "Error closing file:", errors
endif

! Clean up our curling mess.
CALL curl_easy_cleanup(handle)
CALL curl_global_cleanup()

END PROGRAM use_curl
