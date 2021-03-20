# Try to find libev
# Once done, this will define
#
# LIBEV_FOUND        - system has libev
# LIBEV_INCLUDE_DIRS - libev include directories
# LIBEV_LIBRARIES    - libraries needed to use libev


#if(LIBEV_INCLUDE_DIRS AND LIBEV_LIBRARIES)
#    set(LIBEV_FIND_QUIETLY TRUE)
#else()

find_path(
    LIBEV_INCLUDE_DIRS
    NAMES ev.h
    HINTS ${LIBEV_ROOT_DIR}
    PATH_SUFFIXES include)

find_library(
    LIBEV_LIBRARIES
    NAME ev
    HINTS ${LIBEV_ROOT_DIR}
    PATH_SUFFIXES ${CMAKE_INSTALL_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    LibEv DEFAULT_MSG LIBEV_LIBRARIES LIBEV_INCLUDE_DIRS)

mark_as_advanced(LIBEV_LIBRARIES LIBEV_INCLUDE_DIRS)

#endif()
#
#if (LIBEV_LIBRARIES)
#    set(LIBEV_FOUND CACHE INTERNAL "Found LibEv")
#endif()
