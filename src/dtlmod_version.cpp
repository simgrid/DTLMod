/* Copyright (c) 2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/version.hpp"

void dtlmod_version_get(int* ver_major, int* ver_minor, int* ver_patch)
{
  *ver_major = DTLMOD_VERSION_MAJOR;
  *ver_minor = DTLMOD_VERSION_MINOR;
  *ver_patch = DTLMOD_VERSION_PATCH;
}
