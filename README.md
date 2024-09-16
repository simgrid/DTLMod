[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)

# DTLMod: a versatile simulated Data Transport Layer

## Overview
This project implements a versatile simulated data transport layer "module" on top of
[SimGrid](https://simgrid.frama.io/simgrid/), to be used in any SimGrid-based simulator.

## Dependencies and installation

The only required dependencies are on [SimGrid](https://simgrid.frama.io/simgrid/) and its
[File System Module](https://github.com/simgrid/file-system-module). An optional dependency
is [Google Test](https://github.com/google/googletest) for compiling the unit tests.

Here is the typical Ubuntu installation procedure:

```bash
cd DTL
mkdir build
cd build
cmake ..
make -j4
sudo make install
```

after which the dtlmod library and header files will be installed in `/usr/local/`.

To compile and run the unit tests, just run the following command in the `build` directory:

```bash
make unit_tests
./unit_tests
```

---
