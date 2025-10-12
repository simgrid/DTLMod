.. Copyright 2025

.. _install:

Installing DTLMod
=================

.. _install_src:

Installing from the Source
--------------------------

.. _install_src_deps:

Getting the Dependencies
^^^^^^^^^^^^^^^^^^^^^^^^

C++ compiler
 Install a version recent enough to support the C++17 standard.
Python 3
  DTLMod should build without Python which is only needed by the regression tests of the python API.
cmake
  The minimal recommended version is v3.12
boost mandatory components (at least v1.48)
  - On Debian / Ubuntu: ``apt install libboost-dev``
  - On CentOS / Fedora: ``dnf install boost-devel``
  - On macOS with homebrew: ``brew install boost``
python bindings (optional):
  - On Debian / Ubuntu: ``apt install pybind11-dev python3-dev``
JSON:
  - On Debian / Ubuntu: ``apt install nlohmann-json3-dev``

^^^^^^^^^^^^^^^^^^^
Getting the Sources
^^^^^^^^^^^^^^^^^^^

You can find the latest **stable release** on  `Github
<https://github.com/simgrid/DTLMod/-/releases>`_, and compile it as follows:

.. code-block:: console

   $ tar xf simgrid-3-XX.tar.gz
   $ cd simgrid-*
   $ cmake -DCMAKE_INSTALL_PREFIX=/opt/simgrid -GNinja .
   $ ninja                     # or 'make' if you remove the '-GNinja' above
   $ ninja install

If you want to stay on the **bleeding edge**, get the current git version,
and recompile it as with stable archives. You may need some extra
dependencies.
