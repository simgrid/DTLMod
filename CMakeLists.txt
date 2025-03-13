cmake_minimum_required(VERSION 3.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/conf/cmake/")
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

project(dtlmod VERSION 0.1 DESCRIPTION "Data Transport Layer Module")

include(GNUInstallDirs)
find_package(SimGrid 4.0 REQUIRED)
find_package(FSMod 0.3 REQUIRED)
find_package(nlohmann_json REQUIRED)

include_directories(
  ${CMAKE_SOURCE_DIR}/include
	${SimGrid_INCLUDE_DIR}
  ${FSMod_INCLUDE_DIR}
  ${CMAKE_BINARY_DIR}/include
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-O3 -funroll-loops -fno-strict-aliasing ")

# build the version number
set(DTLMOD_VERSION_MAJOR "0")
set(DTLMOD_VERSION_MINOR "1")
set(DTLMOD_VERSION_PATCH "0")
set(DTLMOD_VERSION_EXTRA "dev")

if (${DTLMOD_VERSION_PATCH} EQUAL "0")
    set(DTLMOD_RELEASE_VERSION "${DTLMOD_VERSION_MAJOR}.${DTLMOD_VERSION_MINOR}")
else ()
    set(DTLMOD_RELEASE_VERSION "${DTLMOD_VERSION_MAJOR}.${DTLMOD_VERSION_MINOR}.${DTLMOD_VERSION_PATCH}")
endif ()

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
  src/MailboxTransport.cpp
  src/MessageQueueTransport.cpp
)

set(HEADER_FILES
  include/dtlmod/DTL.hpp
  include/dtlmod/DTLException.hpp
  include/dtlmod/Engine.hpp
  include/dtlmod/FileTransport.hpp
  include/dtlmod/FileEngine.hpp
  include/dtlmod/MailboxTransport.hpp
  include/dtlmod/MessageQueueTransport.hpp
  include/dtlmod/Metadata.hpp
  include/dtlmod/StagingEngine.hpp
  include/dtlmod/Stream.hpp
  include/dtlmod/Transport.hpp
  include/dtlmod/Variable.hpp)

set (CONFIG_FILES
  test/DTL-config.json)

add_library(dtlmod SHARED ${SOURCE_FILES})

set_target_properties(dtlmod PROPERTIES
  SOVERSION 0.1
  LINKER_LANGUAGE CXX
  PUBLIC_HEADER "${HEADER_FILES}")

target_include_directories(dtlmod PRIVATE include)
target_link_libraries(dtlmod PUBLIC ${SimGrid_LIBRARY} ${FSMOD_LIBRARY})

foreach (conf "${CONFIG_FILES}")
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
	target_include_directories(unit_tests PRIVATE include)
	target_link_libraries(unit_tests ${GTEST_LIBRARY} ${SIMGRID_LIBRARY} ${FSMOD_LIBRARY} dtlmod -lpthread -lm)
	set_target_properties(unit_tests PROPERTIES COMPILE_FLAGS "-g -O0 --coverage")
	set_target_properties(unit_tests PROPERTIES LINK_FLAGS "--coverage")
	#add_custom_command(TARGET unit_tests COMMAND find . -name *.gcda -delete)
else()
	add_custom_target(unit_tests echo "ERROR: Cannot build unit_tests because Google Test (libgtest) was not found by cmake." COMMAND echo "       If you have installed Google Test, re-run cmake." VERBATIM)
endif()

# Documentation
include(${CMAKE_HOME_DIRECTORY}/conf/cmake/Documentation.cmake)
