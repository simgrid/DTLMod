#include <gtest/gtest.h>
#include <xbt.h>

#include <cstring>
#include <dlfcn.h>
#include <iostream>

#include "dtlmod/DTL.hpp"

/// Make sure the tests exercise the library that was just built, and not another copy of it found earlier in the
/// loader's search path. LD_LIBRARY_PATH takes precedence over the DT_RUNPATH baked into this binary, so an installed
/// libdtlmod in one of its entries silently wins over the one in the build tree, and the whole suite then reports on
/// code that is not the one being modified.
static bool running_against_built_library()
{
  Dl_info info;
  if (dladdr(reinterpret_cast<void*>(&dtlmod::DTL::create), &info) == 0 || info.dli_fname == nullptr)
    return true; // Cannot tell, do not get in the way.

  if (std::strstr(info.dli_fname, DTLMOD_BUILT_LIB_DIR) != nullptr)
    return true;

  std::cerr << "FATAL: the tests are loading " << info.dli_fname << "\n"
            << "       instead of the library built in " << DTLMOD_BUILT_LIB_DIR << "\n"
            << "       Prepend that directory to LD_LIBRARY_PATH before running the tests.\n";
  return false;
}

int main(int argc, char** argv)
{
  if (!running_against_built_library())
    return 1;

  // disable log
  xbt_log_control_set("no_loc");
  xbt_log_control_set("root.fmt=[%2.6r]%e[%a@%h]%e%m%n");
  xbt_log_control_set("root.thresh:info");
  // Activate dtlmod debug for all tests
  // xbt_log_control_set("dtlmod.thresh:debug");
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
