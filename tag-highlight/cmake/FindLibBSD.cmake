#
# - Try to find libbsd
# Once done this will define
#
#  LibBSD_FOUND - system has libbsd
#  LibBSD_INCLUDE_DIRS - the libbsd include directory
#  LibBSD_LIBRARIES - Link these to use libbsd
#  LibBSD_DEFINITIONS - Compiler switches required for using libbsd
#
#  Copyright (c) 2010 Holger Hetterich <hhetter@novell.com>
#  Copyright (c) 2007 Andreas Schneider <mail@cynapses.org>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if (LibBSD_LIBRARIES AND LibBSD_INCLUDE_DIRS)
    # in cache already
    set (LibBSD_FOUND TRUE)
else()

    find_path(
        LibBSD_INCLUDE_DIR
        NAMES bsd/bsd.h
        PATHS /usr/include /usr/local/include /opt/local/include /sw/include
    )

    find_library(
        LibBSD_LIBRARY
        NAMES bsd
        PATHS /usr/lib /usr/local/lib /opt/local/lib /sw/lib
    )

    if (LibBSD_LIBRARY)
        set(LibBSD_FOUND TRUE)
    endif()

    set(LibBSD_INCLUDE_DIRS ${INIPARSER_INCLUDE_DIR})

    if (LibBSD_FOUND)
        set (LibBSD_LIBRARIES
             ${LibBSD_LIBRARIES} ${LibBSD_LIBRARY}
        )
    endif()

    if (LibBSD_INCLUDE_DIRS AND LibBSD_LIBRARIES)
        set(LibBSD_FOUND TRUE)
    endif()

    if (LibBSD_FOUND)
        if (NOT LibBSD_FIND_QUIETLY)
            message(STATUS "Found LibBSD: ${LibBSD_LIBRARIES}")
        endif()
    else()
        if (LibBSD_FIND_REQUIRED)
            message(FATAL_ERROR "Could not find libbsd")
        endif()
    endif()

    # show the INIPARSER_INCLUDE_DIRS and INIPARSER_LIBRARIES
    # variables only in the advanced view
    mark_as_advanced(LibBSD_INCLUDE_DIRS LibBSD_LIBRARIES)

endif()
