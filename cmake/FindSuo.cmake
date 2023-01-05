# - Try to find Suo
# Once done this will define
# Suo_FOUND - System has Suo
# Suo_INCLUDE_DIRS - The Suo include directories
# Suo_LIBRARIES - The libraries needed to use Suo
# Suo_DEFINITIONS - Compiler switches required for using Suo

include(GNUInstallDirs)


find_path( Suo_INCLUDE_DIRS
    suo.hpp
    HINTS ${SUO_GIT}/libsuo
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/suo/libsuo
)

find_library( Suo_LIBRARIES
    NAMES suo
    HINTS ${SUO_GIT}/build/libsuo
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/suo/build/libsuo
)

include ( FindPackageHandleStandardArgs )
find_package_handle_standard_args(Suo DEFAULT_MSG Suo_LIBRARIES Suo_INCLUDE_DIRS )
