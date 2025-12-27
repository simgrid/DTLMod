.. Copyright 2025-2026

.. _install:

Installing DTLMod
=================

.. _install_src:

Installing from source
----------------------

.. _install_src_deps:

Getting the dependencies
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

Getting the source
^^^^^^^^^^^^^^^^^^

You can find the latest **stable release** on  `Github
<https://github.com/simgrid/DTLMod/releases>`_, and compile it as follows.

We recommend an **out-of-source** build where all the files produced during the compilation are placed in a separate
directory. Cleaning the building tree then becomes as easy as removing this directory, and you can have several such
directories to test different parameter sets or compiling toolchains.

.. code-block:: console

   $ tar xf v0.X.tar.gz # where X is the minor version number of the release 
   $ cd DTLMod-*
   $ mkdir build
   $ cd build 
   $ cmake ..
   $ make
   $ sudo make install
   $ cd ..

To benefit of the latest developments, get the current git version and recompile it as with stable archives. 

.. code-block:: console

   $ git clone https://github.com/simgrid/DTLMod.git
   $ cd DTLMod
   $ mkdir build
   $ cd build 
   $ cmake ..
   $ make
   $ sudo make install
   $ cd ..

Testing your build
^^^^^^^^^^^^^^^^^^

Once everything is built, you may want to test the result. DTLMod comes with an extensive test suite which requires to
install ``googletest`` first. These tests are not built by default. You first have to build them with 
``make unit_tests``. You can then run them with ``./unit_tests``. Additionally, the Python API can also be tested as
follows:

.. code-block:: console

   $ cd test/python
   $ python unit_tests_python.pymkdir build
   $ cd - 

We run both test suites on every commit as part of a **github action** and the results can be found 
`there <https://github.com/simgrid/DTLMod/actions>`_.  

Python-specific instructions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Once you have the Python development headers installed as well as a recent version of `pybind11 <https://pybind11.readthedocs.io/en/stable/>`_
(version at least 2.4), recompiling the Python bindings from the source amounts to:

.. code-block:: console

  # cd DTLMod-source-tree
  $ python setup.py build install

.. SimGrid 3.13, it should even be possible to install simgrid without downloading the source with pip:

.. .. code-block:: console

..  $ pip install simgrid

You may have to tell Python where to find the libraries (DTLMod, FSMod, and SimGrid), e.g., if you installed them in a
non-standard directory, as follows:

.. code-block:: console

  $ PYTHONPATH="/usr/local/lib/python3.13/dist-packages" LD_LIBRARY_PATH="/usr/local/lib" python your_script.py

You can add those variables to your bash profile to not specify it each time by adding these lines to your ``~/.profile``:

.. code-block:: console

  export PYTHONPATH="$PYTHONPATH:/usr/local/lib/python3.13/dist-packages"
  export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/usr/local/lib"
