[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)
[![Build-Linux](https://github.com/simgrid/DTLMod/actions/workflows/build.yml/badge.svg)](https://github.com/simgrid/DTLMod/actions/workflows/build.yml)
[![Doc](https://readthedocs.org/projects/pip/badge/?version=stable)](https://simgrid.github.io/DTLMod/)
[![SonarCloud Status](https://sonarcloud.io/api/project_badges/measure?project=simgrid_dtlmod&metric=alert_status)](https://sonarcloud.io/summary/new_code/?id=simgrid_dtlmod)
[![codecov](https://codecov.io/gh/simgrid/DTLMod/graph/badge.svg?token=6x9KmpEvpS)](https://codecov.io/gh/simgrid/DTLMod)
[![CodeFactor](https://www.codefactor.io/repository/github/simgrid/DTLMod/badge)](https://www.codefactor.io/repository/github/simgrid/DTLMod)

# DTLMod: a versatile simulated Data Transport Layer

## Overview
This project implements a versatile simulated data transport layer "module" on top of
[SimGrid](https://simgrid.frama.io/simgrid/), to be used in any SimGrid-based simulator.

## Documentation

The concepts and API of DTLMod API are documented on [this page](https://simgrid.github.io/DTLMod/).

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

## Citation

If you use this software in your research, please cite:
```bibtex
@inproceedings{dtlmod,
  title = {{A Versatile Simulated Data Transport Layer for In Situ
  Workflows Performance Evaluation}},
  author = {Suter, Frédéric},
  booktitle = {Proceedings of the 27th IEEE International Conference on
  Cluster Computing},
  address = {Edinburgh, Scotland},
  year = {2025},
  month = sep,
  doi = {10.1109/CLUSTER59342.2025.11186460},
  series = {Cluster}
}
```

This article comes with a complete experimental artifact that can be
found on [figshare](https://doi.org/10.6084/m9.figshare.28872509.v1).

---
