.. Copyright 2025

.. _start_new_project:

Start a new project
===================

It is not advised to modify the source code of DTLMod directly, as it would make it difficult to upgrade to the next
version of the library. Instead, you should create your own working directory somewhere on your disk (e.g.,
``~home/MySimulator/``), and develop your simulate in there.


Building your project with CMake
--------------------------------

Here follows a ``CMakeLists.txt`` file that you can use as a starting point for your DTLMod-based project. It builds a
simulator from a given set of source files.

.. code-block:: cmake

    cmake_minimum_required(VERSION 3.12)
    project(MySimulator)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -funroll-loops -fno-strict-aliasing -flto=auto")

    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
    find_package(SimGrid 4.1 REQUIRED)
    find_package(FSMod 0.4 REQUIRED)
    find_package(DTLMod REQUIRED)
 
    include_directories(
        ${SimGrid_INCLUDE_DIR}
        ${FSMod_INCLUDE_DIR}
    )

    set(SOURCE_FILES
        main.c
        other.c
        util.c
    )

    add_executable(my_simulator ${SOURCE_FILES})
    target_link_libraries(my_simulator ${DTLMOD_LIBRARY} ${FSMOD_LIBRARY} ${SIMGRID_LIBRARY})


For that, you need to copy different CMake modules in the ``cmake/Modules`` directory of your project:

  - `FindSimGrid.cmake <https://framagit.org/simgrid/simgrid/raw/master/FindSimGrid.cmake>`_
  - `FindFSMod.cmake <https://github.com/simgrid/file-system-module/blob/main/FindFSMod.cmake>`_
  - `FindDTLMod.cmake <https://github.com/simgrid/DTLMod/blob/main/FindDTLMod.cmake>`_

Note that these files have to be kept up to date, but their update frequency should be rather low.
