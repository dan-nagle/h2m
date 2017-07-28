! Very small program to make a GET
! request with curl.
PROGRAM use_curl
use, intrinsic :: iso_c_binding
use module_curl
implicit none
integer(c_int) errors
TYPE(C_PTR), TARGET :: handle
CHARACTER(Kind=C_CHAR, len=20), TARGET :: url
integer(c_long) :: CURL_GLOBAL_ALL = 3

errors = curl_global_init(CURL_GLOBAL_ALL)

! Let's get ourselves a handle!
handle = curl_easy_init()
if (.NOT. C_ASSOCIATED(handle)) then
  WRITE(*,*) "No pointer."
else
  WRITE(*,*) "Establishing handle."
  ! Attempt to set the URL to google.
  url = "http://google.com" // C_NULL_CHAR
  errors = curl_easy_setopt(handle, CURLOPT_URL, C_LOC(url))

  if (errors .NE. 0) then
    WRITE(*,*) "Error for you sir:", errors
  else
    ! Attempt to do the curl thing.
    errors = curl_easy_perform(handle)
    if (errors .NE. 0) then
      WRITE(*,*) curl_easy_strerror(errors)
    else
      WRITE(*,*) "Cleaning up Curl."
    endif
  endif

endif


! Clean up our curling mess.
CALL curl_easy_cleanup(handle)
CALL curl_global_cleanup()

END PROGRAM use_curl
