# Copyright (c) 2025. The SWAT Team. All rights reserved.

# This program is free software; you can redistribute it and/or modify it
# under the terms of the license (GNU LGPL-2.1-only) which comes with this package.

# python3 -m build --sdist # Build a source distrib (building binary distribs is complex on linux)

# twine upload --repository-url https://test.pypi.org/legacy/ dist/dtlmod-*.tar.gz # Upload to test
# pip3 install --extra-index-url https://test.pypi.org/simple  dtlmod

# Once it works, upload to the real infra.  /!\ you cannot modify a file once uploaded
# twine upload dist/dtlmod-*.tar.gz

import os
import sys
import subprocess
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        super().__init__(name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)

class CMakeBuild(build_ext):
    def run(self):
        try:
            subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError("CMake must be installed to build the dtlmod extension")

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        from pybind11 import get_cmake_dir

        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={extdir}",
            f"-DPYTHON_EXECUTABLE={sys.executable}",
            "-Denable_python=ON",
            f"-Dpybind11_DIR={get_cmake_dir()}",
        ]

        build_temp = self.build_temp
        os.makedirs(build_temp, exist_ok=True)

        subprocess.check_call(["cmake", ext.sourcedir] + cmake_args, cwd=build_temp)
        subprocess.check_call(["cmake", "--build", "."], cwd=build_temp)

setup(
    ext_modules=[CMakeExtension("dtlmod")],
    cmdclass={"build_ext": CMakeBuild},
)
