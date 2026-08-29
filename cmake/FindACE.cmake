#
# Find the ACE client includes and library
#

# This module defines
# ACE_INCLUDE_DIR, where to find ace.h
# ACE_LIBRARIES, the libraries to link against
# ACE_FOUND, if false, you cannot build anything that requires ACE

# also defined, but not for general use are
# ACE_LIBRARY, where to find the ACE library.

set(ACE_FOUND 0)

if (UNIX)

    # Debian and Ubuntu install ACE libraries in their multiarch libdir (for
    # example, /usr/lib/x86_64-linux-gnu). Use ACE.pc when available so this
    # also works with CMake cache entries created without compiler metadata.
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(PC_ACE QUIET ACE)
    endif()

    # Some Ubuntu ACE.pc files report /usr/lib even though the package puts
    # libACE.so in the Debian multiarch directory. Recover that directory when
    # an older or externally-created CMake cache has no architecture metadata.
    execute_process(
        COMMAND dpkg-architecture -qDEB_HOST_MULTIARCH
        OUTPUT_VARIABLE DEB_HOST_MULTIARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    FIND_PATH(ACE_INCLUDE_DIR
    NAMES
      ace/ACE.h
    HINTS
      ${PC_ACE_INCLUDE_DIRS}
      PATHS
      /usr/include
      /usr/include/ace
      /usr/local/include
      /usr/local/include/ace
      ${ACE_ROOT}
      ${ACE_ROOT}/include
      $ENV{ACE_ROOT}
      $ENV{ACE_ROOT}/include
    DOC "Specify include-directories that might contain ace.h here.")

    FIND_LIBRARY(ACE_LIBRARIES
    NAMES
      ace ACE
    HINTS
      ${PC_ACE_LIBRARY_DIRS}
    PATHS
      /usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}
      /lib/${CMAKE_LIBRARY_ARCHITECTURE}
      /usr/lib/${DEB_HOST_MULTIARCH}
      /lib/${DEB_HOST_MULTIARCH}
      /usr/lib
      /usr/lib/ace
      /usr/local/lib
      /usr/local/lib/ace
      /usr/local/ace/lib
      ${ACE_ROOT}
      ${ACE_ROOT}/lib
      $ENV{ACE_ROOT}/lib
      $ENV{ACE_ROOT}
    DOC "Specify library-locations that might contain the ACE library here.")
endif (UNIX)

if (WIN32)

    FIND_PATH(ACE_INCLUDE_DIR
    NAMES
      ace/ACE.h
    PATHS
      ${ACE_ROOT}
      ${ACE_ROOT}/include
      $ENV{ACE_ROOT}
      $ENV{ACE_ROOT}/include
    DOC "Specify include-directories that might contain ace.h here.")

    FIND_LIBRARY(ACE_LIBRARIES
    NAMES
      ace ACE ACEd
    PATHS
      ${ACE_ROOT}
      ${ACE_ROOT}/lib
      $ENV{ACE_ROOT}/lib
      $ENV{ACE_ROOT}
    DOC "Specify library-locations that might contain the ACE library here.")

endif (WIN32)

if (ACE_LIBRARIES)
    if (ACE_INCLUDE_DIR)
        set(ACE_FOUND 1)
        message(STATUS "Found ACE library: ${ACE_LIBRARIES}")
        message( STATUS "Found ACE headers: ${ACE_INCLUDE_DIR}")
    else (ACE_INCLUDE_DIR)
        message(FATAL_ERROR "Could not find ACE headers! Please install ACE libraries and headers")
    endif (ACE_INCLUDE_DIR)
endif (ACE_LIBRARIES)

mark_as_advanced(ACE_FOUND ACE_LIBRARIES ACE_INCLUDE_DIR)
