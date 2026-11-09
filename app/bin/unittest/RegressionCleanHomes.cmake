# RegressionCleanHomes (RegressionTests.cmake): remove every regression test's
# home directory, then create each one again, empty. Run with cmake -P.
#
#   -DROOT=<dir>   the directory that holds all the homes; removed first
#   -DLIST=<file>  the homes to create, one path per line
#
# The program creates only its work directory inside $HOME, never $HOME
# itself, and treats a missing $HOME as fatal (see RegressionTests.cmake).

if(NOT ROOT OR NOT LIST)
    message(FATAL_ERROR "RegressionCleanHomes.cmake: ROOT and LIST are required")
endif()

file(REMOVE_RECURSE "${ROOT}")
file(STRINGS "${LIST}" _homes)
list(LENGTH _homes _count)
if(_count EQUAL 0)
    message(FATAL_ERROR "No home directories listed in ${LIST}")
endif()
file(MAKE_DIRECTORY ${_homes})
message(STATUS "Created ${_count} empty home directories under ${ROOT}")
