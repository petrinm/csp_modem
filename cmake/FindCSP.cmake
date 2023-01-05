# - Try to find CSP
# Once done this will define
# CSP_FOUND - System has CSP
# CSP_INCLUDE_DIRS - The CSP include directories
# CSP_LIBRARIES - The libraries needed to use CSP
# CSP_DEFINITIONS - Compiler switches required for using CSP

find_path ( CSP_INCLUDE_DIRS
    csp/csp.h
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/libcsp/install/include
)

find_library ( CSP_LIBRARIES
    NAMES csp
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/libcsp/install/lib    
)

# handle the QUIETLY and REQUIRED arguments and set CSP_FOUND to TRUE
# if all listed variables are TRUE
include ( FindPackageHandleStandardArgs )
find_package_handle_standard_args ( CSP DEFAULT_MSG CSP_LIBRARIES CSP_INCLUDE_DIRS )
