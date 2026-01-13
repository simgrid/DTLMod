cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/conf/cmake/")
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(dtlmod VERSION 0.2 DESCRIPTION "Data Transport Layer Module")

include(GNUInstallDirs)
find_package(Boost 1.48)
find_package(SimGrid 4.1 REQUIRED)
find_package(FSMod 0.4 REQUIRED)
find_package(nlohmann_json REQUIRED)

# Note: Global include_directories removed in favor of target-specific includes below

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -funroll-loops -fno-strict-aliasing -flto=auto -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wall -Wextra")

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# Sanitizer support
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_VALGRIND "Build with Valgrind support (without sanitizers)" OFF)

if(ENABLE_VALGRIND)
  if(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_TSAN)
    message(FATAL_ERROR "Cannot enable both Valgrind and sanitizers. Please choose one.")
  endif()
  message(STATUS "Building with Valgrind support")
  add_definitions(-DVALGRIND_BUILD)
endif()

if(ENABLE_ASAN)
  message(STATUS "Building with AddressSanitizer")
  set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=address")
endif()

if(ENABLE_UBSAN)
  message(STATUS "Building with UndefinedBehaviorSanitizer")
  set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
  set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=undefined")
endif()

if(ENABLE_TSAN)
  message(STATUS "Building with ThreadSanitizer")
  set(SANITIZER_FLAGS "${SANITIZER_FLAGS} -fsanitize=thread -fno-omit-frame-pointer")
  set(SANITIZER_LINK_FLAGS "${SANITIZER_LINK_FLAGS} -fsanitize=thread")
endif()

if(SANITIZER_FLAGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_LINK_FLAGS}")
endif()

# build the version number
set(DTLMOD_VERSION_MAJOR "0")
set(DTLMOD_VERSION_MINOR "2")
set(DTLMOD_VERSION_PATCH "0")
set(DTLMOD_VERSION_EXTRA "dev")

## GIT version check
####################

if(EXISTS ${CMAKE_HOME_DIRECTORY}/.git/)
  execute_process(COMMAND git rev-parse --verify --short HEAD
     WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
     OUTPUT_VARIABLE GIT_VERSION
     OUTPUT_STRIP_TRAILING_WHITESPACE)
  # Check for uncommitted changes
  execute_process(COMMAND git diff --name-only HEAD
    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
    OUTPUT_VARIABLE files_changed)
  if(files_changed)
    set(GIT_VERSION "${GIT_VERSION}-dirty")
  endif()
elseif(EXISTS ${CMAKE_HOME_DIRECTORY}/.gitversion)
  FILE(STRINGS ${CMAKE_HOME_DIRECTORY}/.gitversion GIT_VERSION)
else()
  set(GIT_VERSION "none, release version")
endif()
message(STATUS "Git version: ${GIT_VERSION}")

if (${DTLMOD_VERSION_PATCH} EQUAL "0")
    set(DTLMOD_RELEASE_VERSION "${DTLMOD_VERSION_MAJOR}.${DTLMOD_VERSION_MINOR}")
else ()
    set(DTLMOD_RELEASE_VERSION "${DTLMOD_VERSION_MAJOR}.${DTLMOD_VERSION_MINOR}.${DTLMOD_VERSION_PATCH}")
endif ()

### Generate the required headers
#################################

# Avoid triggering a (full) rebuild by touching the files if they did not really change
configure_file("${CMAKE_HOME_DIRECTORY}/include/dtlmod/version.hpp.in" "${CMAKE_BINARY_DIR}/include/dtlmod/version.hpp.generated" @ONLY IMMEDIATE)
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/include/dtlmod/version.hpp.generated ${CMAKE_BINARY_DIR}/include/dtlmod/version.hpp)
file(REMOVE ${CMAKE_BINARY_DIR}/include/dtlmod/version.hpp.generated)


# Python binding (with pybind11)
################################
if((NOT DEFINED enable_python) OR enable_python)
  find_package(Python3 COMPONENTS Interpreter)
  if(NOT Python3_Interpreter_FOUND)
    message(FATAL_ERROR "Please install Python (version 3 or higher) to compile DTLMod Python bindings.")
  endif() 
  set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})

  find_package(pybind11 CONFIG)
  message(STATUS "Pybind11 version: ${pybind11_VERSION}")
  if (pybind11_VERSION VERSION_LESS 2.4)
    message(STATUS "DTLMod needs at least v2.4 of pybind11. Disabling the Python bindings.")
    set(pybind11_FOUND OFF)
  endif()
endif()

find_package(Python3 COMPONENTS Development)
if(NOT Python3_Development_FOUND OR NOT pybind11_FOUND)
  message(STATUS "DTLMod Python bindings cannot be built on this system.")
  set(default_enable_python OFF)
else()
  set(default_enable_python ON)
endif()

option(enable_python "Whether the Python bindings are activated." ${default_enable_python}) # ON by default if dependencies are met

if(enable_python)
  if(NOT Python3_Development_FOUND)
    message(FATAL_ERROR "Please install the development components of Python (python3-dev on Debian) to build the Python bindings (or disable that option).")
  endif()
  if(pybind11_FOUND)
    message(STATUS "Python bindings will be built.")
    set(python_files ${CMAKE_SOURCE_DIR}/src/bindings/python/dtlmod_python.cpp)

    message(STATUS "Files to compile: ${python_files}")
    pybind11_add_module(python-bindings
                        ${python_files}
                        ${pybind11_options})

    target_compile_features(python-bindings PRIVATE cxx_std_17)
    target_link_libraries(python-bindings PUBLIC dtlmod)
    set_target_properties(python-bindings PROPERTIES
                          LIBRARY_OUTPUT_NAME dtlmod
                          CXX_VISIBILITY_PRESET "default"
                          INTERPROCEDURAL_OPTIMIZATION FALSE)
   
    set_property(TARGET python-bindings
                 APPEND PROPERTY INCLUDE_DIRECTORIES "${INTERNAL_INCLUDES}")

    if("${DTLMOD_PYTHON_LIBDIR}" STREQUAL "") # value not manually set
      if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
        set(DTLMOD_PYTHON_LIBDIR ${Python3_SITEARCH})
      else("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
        string(REGEX REPLACE "^/usr/local/" "${CMAKE_INSTALL_PREFIX}/" DTLMOD_PYTHON_LIBDIR ${Python3_SITEARCH})
      endif("${CMAKE_INSTALL_PREFIX}" STREQUAL "/usr")
    endif()
    install(TARGETS python-bindings
      	    LIBRARY DESTINATION "${DTLMOD_PYTHON_LIBDIR}")
  else()
    message(FATAL_ERROR "Please install pybind11-dev to build the Python bindings (or disable that option).")
  endif()
endif()
mark_as_advanced(PYBIND11_PYTHON_VERSION)
mark_as_advanced(pybind11_DIR)

set(SOURCE_FILES
  src/DTL.cpp
  src/Engine.cpp
  src/FileEngine.cpp
  src/StagingEngine.cpp
  src/Metadata.cpp
  src/Stream.cpp
  src/Variable.cpp
  src/Transport.cpp
  src/FileTransport.cpp
  src/StagingMboxTransport.cpp
  src/StagingMqTransport.cpp
  src/StagingTransport.cpp
  src/dtlmod_version.cpp
)

set(HEADER_FILES
  include/dtlmod/ActorRegistry.hpp
  include/dtlmod/DTL.hpp
  include/dtlmod/DTLException.hpp
  include/dtlmod/Engine.hpp
  include/dtlmod/FileEngine.hpp
  include/dtlmod/FileTransport.hpp
  include/dtlmod/Metadata.hpp
  include/dtlmod/StagingEngine.hpp
  include/dtlmod/StagingMboxTransport.hpp
  include/dtlmod/StagingMqTransport.hpp
  include/dtlmod/StagingTransport.hpp
  include/dtlmod/Stream.hpp
  include/dtlmod/Transport.hpp
  include/dtlmod/Variable.hpp
  include/dtlmod/version.hpp.in
)

set (CONFIG_FILES
  test/DTL-config.json)

add_library(dtlmod SHARED ${SOURCE_FILES})

set_target_properties(dtlmod PROPERTIES
  SOVERSION 0.2
  LINKER_LANGUAGE CXX
  PUBLIC_HEADER "${HEADER_FILES}")

target_include_directories(dtlmod
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
    ${SimGrid_INCLUDE_DIR}
    ${FSMOD_INCLUDE_DIR}
)
target_link_libraries(dtlmod PUBLIC ${SimGrid_LIBRARY} ${FSMOD_LIBRARY})

foreach (conf ${CONFIG_FILES})
  add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_CURRENT_SOURCE_DIR}/${conf}
      ${CMAKE_CURRENT_BINARY_DIR}/config_files/${conf})
endforeach()

install(TARGETS dtlmod
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dtlmod)
install(FILES include/dtlmod.hpp DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Google test
find_library(GTEST_LIBRARY NAMES gtest)
find_path(GTEST_INCLUDE_DIR NAMES gtest/gtest.h PATHS /opt/gtest/include)
if(GTEST_LIBRARY)
  set(TEST_FILES
      test/dtl_config.cpp
      test/dtl_connection.cpp
      test/dtl_file_engine.cpp
      test/dtl_staging_engine.cpp
      test/dtl_stream.cpp
      test/dtl_variable.cpp
      test/main.cpp
      test/test_util.hpp
      include/dtlmod.hpp)

  add_definitions(-DGTEST_USED)
  add_executable(unit_tests EXCLUDE_FROM_ALL ${SOURCE_FILES} ${HEADER_FILES} ${TEST_FILES})
  target_include_directories(unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${SimGrid_INCLUDE_DIR}
    ${FSMOD_INCLUDE_DIR}
    ${GTEST_INCLUDE_DIR}
  )
  target_link_libraries(unit_tests ${GTEST_LIBRARY} ${SimGrid_LIBRARY} ${FSMOD_LIBRARY} dtlmod -lpthread -lm)
  set_target_properties(unit_tests PROPERTIES COMPILE_FLAGS "-g -O0 --coverage")
  set_target_properties(unit_tests PROPERTIES LINK_FLAGS "--coverage")
  #add_custom_command(TARGET unit_tests COMMAND find . -name *.gcda -delete)

  # Python test - copy test scripts
  add_custom_target(copy_python_tests
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/test/python
      COMMAND ${CMAKE_COMMAND} -E copy_directory
              ${CMAKE_SOURCE_DIR}/test/python
              ${CMAKE_BINARY_DIR}/test/python
      COMMENT "Copying Python test scripts to build directory"
  )
  add_dependencies(unit_tests copy_python_tests)

  # Build Python bindings when building unit_tests
  if(enable_python AND TARGET python-bindings)
    add_dependencies(unit_tests python-bindings)
  endif()

  # Valgrind target
  find_program(VALGRIND_EXECUTABLE valgrind)
  if(VALGRIND_EXECUTABLE)
    add_custom_target(valgrind
      COMMAND ${VALGRIND_EXECUTABLE}
        --leak-check=full
        --show-leak-kinds=all
        --track-origins=yes
        --verbose
        --log-file=valgrind-output.txt
        --suppressions=${CMAKE_SOURCE_DIR}/test/valgrind.supp
        $<TARGET_FILE:unit_tests>
      DEPENDS unit_tests
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Running Valgrind memory checks"
    )
    message(STATUS "Valgrind found: ${VALGRIND_EXECUTABLE}")
  else()
    add_custom_target(valgrind
      COMMAND ${CMAKE_COMMAND} -E echo "ERROR: Valgrind not found. Please install valgrind."
      VERBATIM
    )
  endif()
else()
  add_custom_target(unit_tests echo "ERROR: Cannot build unit_tests because Google Test (libgtest) was not found by cmake." COMMAND echo "       If you have installed Google Test, re-run cmake." VERBATIM)
endif()

if(pybind11_FOUND)
  message("        Compile Python bindings .....: ${enable_python}")
  message("          module ....................: ${PYTHON_MODULE_PREFIX}dtlmod${PYTHON_MODULE_EXTENSION}")
  message("          install path ..............: ${DTLMOD_PYTHON_LIBDIR} (force another value with -DDTLMOD_PYTHON_LIBDIR)")
else()
  message("        Compile Python bindings .....: OFF (disabled, or pybind11 not found)")
endif()