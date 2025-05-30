# CMake find module to search for the Data Transport Layer Module.

# Copyright (c) 2024-2025. The SWAT Team.
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL) which comes with this package.

#
# USERS OF PROGRAMS USING DTLMod
# -------------------------------
#
# If cmake does not find this file, add its path to CMAKE_PREFIX_PATH:
#    CMAKE_PREFIX_PATH="/path/to/FindDTLMod.cmake:$CMAKE_PREFIX_PATH"  cmake .
#
# If this file does not find DTLMod, define DTLMOD_PATH:
#    DTLMOD_PATH=/path/to/dtlmod cmake .

#
# DEVELOPERS OF PROGRAMS USING DTLMod
# ------------------------------------
#
#  1. Include this file in your own CMakeLists.txt (before defining any target)
#     by copying it in your development tree.
#
#  2. Afterward, if you have CMake >= 2.8.12, this will define a
#     target called 'DTLMOD::DTLMOD'. Use it as:
#       target_link_libraries(your-simulator DTLMOD::DTLMOD)
#
#    With older CMake (< 2.8.12), it simply defines several variables:
#       DTLMOD_INCLUDE_DIR - the DTLMod include directories
#       DTLMOD_LIBRARY - link your simulator against it to use DTLMod
#    Use as:
#      include_directories("${DTLMOD_INCLUDE_DIR}" SYSTEM)
#      target_link_libraries(your-simulator ${DTLMOD_LIBRARY})
#
#  Since DTLMOD header files require C++17, so we set CMAKE_CXX_STANDARD to 17.
#    Change this variable in your own file if you need a later standard.

cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(DTLMOD_INCLUDE_DIR
        NAMES dtlmod.hpp
        PATHS ${DTLMOD_PATH}/include /opt/dtlmod/include
        )

find_library(DTLMOD_LIBRARY
        NAMES dtlmod
        PATHS ${DTLMOD_PATH}/lib /opt/dtlmod/lib
        )

mark_as_advanced(DTLMOD_INCLUDE_DIR)
mark_as_advanced(DTLMOD_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DTLMod
        FOUND_VAR DTLMOD_FOUND
        REQUIRED_VARS DTLMOD_INCLUDE_DIR DTLMOD_LIBRARY
        VERSION_VAR DTLMOD_VERSION
        REASON_FAILURE_MESSAGE "The DTLMod package could not be located. If you installed DTLMod in a non-standard location, pass -DDTLMOD_PATH=<path to location> to cmake (e.g., cmake -DDTLMOD_PATH=/opt/somewhere/)"
        FAIL_MESSAGE "Could not find the DTLMod installation"
        )


if (DTLMOD_FOUND AND NOT CMAKE_VERSION VERSION_LESS 3.12)

    add_library(DTLMOD::DTLMOD SHARED IMPORTED)
    set_target_properties(DTLMOD::DTLMOD PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${DTLMOD_INCLUDE_DIR}
            INTERFACE_COMPILE_FEATURES cxx_alias_templates
            IMPORTED_LOCATION ${DTLMOD_LIBRARY}
            )
    # We need C++17, so check for it just in case the user removed it since compiling DTLMOD
    if (NOT CMAKE_VERSION VERSION_LESS 3.8)
        # 3.8+ allows us to simply require C++17 (or higher)
        set_property(TARGET DTLMOD::DTLMOD PROPERTY INTERFACE_COMPILE_FEATURES cxx_std_17)
    elseif (NOT CMAKE_VERSION VERSION_LESS 3.1)
        # 3.1+ is similar but for certain features. We pick just one
        set_property(TARGET DTLMOD::DTLMOD PROPERTY INTERFACE_COMPILE_FEATURES cxx_attribute_deprecated)
    else ()
        # Old CMake can't do much. Just check the CXX_FLAGS and inform the user when a C++17 feature does not work
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_CXX_FLAGS}")
        check_cxx_source_compiles("
#if __cplusplus < 201703L
#error
#else
int main(){}
#endif
" _DTLMOD_CXX17_ENABLED)
        if (NOT _DTLMOD_CXX17_ENABLED)
            message(WARNING "C++17 is required to use DTLMod. Enable it with e.g. -std=c++17")
        endif ()
        unset(_DTLMOD_CXX14_ENABLED CACHE)
    endif ()
endif ()
