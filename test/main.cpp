#include <gtest/gtest.h>
#include <xbt.h>

int main(int argc, char** argv)
{
  // disable log
  xbt_log_control_set("no_loc");
  xbt_log_control_set("root.thresh:critical");
  // Activate dtlmod debug for all tests
  // xbt_log_control_set("dtlmod.thresh:debug");
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
