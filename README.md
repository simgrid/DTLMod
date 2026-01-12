# <img src="doc/source/img/DTLMod_logo.png" alt="DTLMod Logo" width="60" style="vertical-align: middle; margin-right:12px;"> DTLMod: a versatile simulated Data Transport Layer

[![License: LGPL v2.1](https://img.shields.io/badge/License-LGPL_v2.1-blue.svg)](https://www.gnu.org/licenses/lgpl-2.1)
[![Build-Linux](https://github.com/simgrid/DTLMod/actions/workflows/build.yml/badge.svg)](https://github.com/simgrid/DTLMod/actions/workflows/build.yml)
[![Doc](https://readthedocs.org/projects/pip/badge/?version=stable)](https://simgrid.github.io/DTLMod/)
[![SonarCloud Status](https://sonarcloud.io/api/project_badges/measure?project=simgrid_dtlmod&metric=alert_status)](https://sonarcloud.io/summary/new_code/?id=simgrid_dtlmod)
[![codecov](https://codecov.io/gh/simgrid/DTLMod/graph/badge.svg?token=6x9KmpEvpS)](https://codecov.io/gh/simgrid/DTLMod)
[![CodeFactor](https://www.codefactor.io/repository/github/simgrid/DTLMod/badge)](https://www.codefactor.io/repository/github/simgrid/DTLMod)

## What is DTLMod?

DTLMod is a high-performance simulation library that provides realistic data transport layer modeling for distributed systems. Built on top of [SimGrid](https://simgrid.org), it enables researchers and developers to accurately simulate complex data movement patterns in HPC workflows, in-situ processing, and distributed computing scenarios without the cost and complexity of physical infrastructure.

### Key Features

- 🚀 **High-fidelity simulation** of data transport protocols
- 🔧 **Flexible API** for easy integration into SimGrid-based simulators
- 📊 **Performance modeling** for in-situ workflows and data movement
- ✅ **Well-tested** with comprehensive unit test coverage
- 📖 **Extensively documented** with examples and API reference

### Use Cases

- Evaluating in-situ workflow performance
- Modeling data-intensive scientific applications
- Testing distributed system architectures
- Optimizing data placement and movement strategies

## Table of Contents

- [What is DTLMod?](#what-is-dtlmod)
- [📖 Documentation](#-documentation)
- [🛠️ Dependencies and Installation](#️-dependencies-and-installation)
- [🤝 Contributing](#-contributing)
- [💬 Support](#-support)
- [📝 Citation](#-citation)

## 📖 Documentation

The complete documentation is available at [simgrid.github.io/DTLMod](https://simgrid.github.io/DTLMod/), including:

### User Manual

- **Getting Started**: Installation and project setup
- **Workflow Design**: Creating simulated in-situ processing workflows with the programming interface
- **Platform Configuration**: Describing simulated platforms

### DTLMod's Internals

- Connection manager architecture
- Engine implementations (File engine and Staging engine)

The documentation provides insights on evaluating performance of different execution scenarios for in-situ workflows at scale.

## 🛠️ Dependencies and Installation

### Prerequisites

**Required:**

- [SimGrid](https://simgrid.frama.io/simgrid/) 4.1 or higher
- [File System Module](https://github.com/simgrid/file-system-module) 0.4 or higher
- [Boost](https://www.boost.org/) 1.48 or higher
- [nlohmann/json](https://github.com/nlohmann/json)
- CMake 3.12 or higher
- C++17 compatible compiler

**Optional:**

- [Google Test](https://github.com/google/googletest) (for C++ unit tests)
- [Python 3](https://www.python.org/) and [pybind11](https://github.com/pybind/pybind11) 2.4 or higher (for Python bindings)

### Building and Installing

Here is the typical installation procedure:

```bash
cd DTLMod
mkdir build
cd build
cmake ..
make -j4
sudo make install
```

The dtlmod library and header files will be installed in `/usr/local/`.

### Running Tests

**C++ unit tests:**

```bash
# From the build directory
make unit_tests
./unit_tests
```

**Python unit tests:**

```bash
# From the build directory
cd test/python
python ./unit_tests_python.py
```

## 🤝 Contributing

Contributions are welcome! Here's how you can help:

- **Report bugs**: Open an issue on the [GitHub issue tracker](https://github.com/simgrid/DTLMod/issues)
- **Submit pull requests**: Fork the repository, make your changes, and submit a PR
- **Improve documentation**: Help us improve examples and API documentation
- **Share use cases**: Let us know how you're using DTLMod in your research

Please ensure your code follows the existing style and includes appropriate tests.

## 💬 Support

If you need help or have questions:

- **Documentation**: Check the [complete documentation](https://simgrid.github.io/DTLMod/) first
- **Issues**: Search existing [GitHub issues](https://github.com/simgrid/DTLMod/issues) or open a new one
- **SimGrid Community**: For general SimGrid questions, visit [simgrid.org](https://simgrid.org/)

## 📝 Citation

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

### Reproducibility Artifact

The complete experimental artifact for this paper, including all datasets, scripts, and instructions for reproducing the results, is available on [figshare](https://doi.org/10.6084/m9.figshare.28872509.v1).

---
