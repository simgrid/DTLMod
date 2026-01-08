/* Copyright (c) 2025-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <simgrid/host.h>
#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include <fsmod/FileSystem.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_reduction, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

class DTLReductionTest : public ::testing::Test {
public:
  sg4::Disk* disk_;
  sg4::Host* host_;

  DTLReductionTest() = default;

  void setup_platform()
  {
    auto* zone = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_empty("zone");
    host_      = zone->add_host("host", "6Gf");
    disk_      = host_->add_disk("disk", "560MBps", "510MBps");
    zone->seal();

    auto my_fs = sgfs::FileSystem::create("my_fs");
    sgfs::FileSystem::register_file_system(zone, my_fs);
    auto local_storage = sgfs::OneDiskStorage::create("local_storage", disk_);
    my_fs->mount_partition("/host/scratch/", local_storage, "100GB");

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLReductionTest, BogusDecimationSetting)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      std::shared_ptr<dtlmod::ReductionMethod> decimator;
      XBT_INFO("Connect to the DTL");
      auto dtl = dtlmod::DTL::connect();
      XBT_INFO("Create a stream");
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      stream->set_metadata_export();
      XBT_INFO("Create a 3D variable");
      auto var = stream->define_variable("var3D", {640, 640, 640}, {0, 0, 0}, {640, 640, 640}, sizeof(double));
      XBT_INFO("Define a unknown Reduction Method, should fail");
      ASSERT_THROW(decimator = stream->define_reduction_method("reduction"), dtlmod::UnknownReductionMethodException);
      XBT_INFO("Define a Decimation Reduction Method");
      ASSERT_NO_THROW(decimator = stream->define_reduction_method("decimation"));
      XBT_INFO("Assign the decimation method to 'var3D' with a bogus option, should fail.");
      ASSERT_THROW(var->set_reduction_operation(decimator, {{"bogus", "-1"}}),
                   dtlmod::UnknownDecimationOptionException);
      XBT_INFO("Assign the decimation method to 'var3D' with only a 2D stride, should fail");
      ASSERT_THROW(var->set_reduction_operation(decimator, {{"stride", "1,2"}}),
                   dtlmod::InconsistentDecimationStrideException);
      XBT_INFO("Assign the decimation method to 'var3D' with a negative stride value, should fail");
      ASSERT_THROW(var->set_reduction_operation(decimator, {{"stride", "1,2,-1"}}),
                   dtlmod::InconsistentDecimationStrideException);
      XBT_INFO("Assign the decimation method to 'var3D' with a stride value of 0, should fail");
      ASSERT_THROW(var->set_reduction_operation(decimator, {{"stride", "1,0,1"}}),
                   dtlmod::InconsistentDecimationStrideException);
      XBT_INFO("Assign the decimation method to 'var3D' with an unknown interpolation method, should fail");
      ASSERT_THROW(var->set_reduction_operation(decimator, {{"stride", "1,2,4"}, {"interpolation", "bogus"}}),
                   dtlmod::UnknownDecimationInterpolationException);

      XBT_INFO("Disconnect the actor from the DTL");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLReductionTest, SimpleDecimationFileEngine)
{
  DO_TEST_WITH_FORK([this]() {
    xbt_log_control_set("dtlmod_engine.thresh:debug");
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      std::shared_ptr<dtlmod::ReductionMethod> decimator;      
      XBT_INFO("Connect to the DTL");
      auto dtl = dtlmod::DTL::connect();
      XBT_INFO("Create a stream");
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      stream->set_metadata_export();
      XBT_INFO("Create a 3D variable");
      auto var =
          stream->define_variable("var3D", {640, 640, 640}, {0, 0, 0}, {640, 640, 640}, sizeof(double));
      XBT_INFO("Define a Decimation Reduction Method");
      ASSERT_NO_THROW(decimator = stream->define_reduction_method("decimation"));
      XBT_INFO("Open the stream in Pulish mode");
      auto engine = stream->open("zone:my_fs:/host/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Start a Transaction");
      engine->begin_transaction();
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var));
      XBT_INFO("End a Transaction");
      engine->end_transaction();
      XBT_INFO("Sleep until t = 6s");
      sg4::this_actor::sleep_until(6);
      XBT_INFO("Assign the decimation method to 'var3D'");
      ASSERT_NO_THROW(var->set_reduction_operation(decimator, {{"stride", "1,2,4"}}));
      XBT_INFO("Check that the variable is marked as 'reduced'");
      ASSERT_TRUE(var->is_reduced());
      XBT_INFO("Start a Transaction");
      engine->begin_transaction();
      XBT_INFO("Put reduced Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var));
      XBT_INFO("End a Transaction");
      engine->end_transaction();
      XBT_INFO("Sleep until t = 8s");
      sg4::this_actor::sleep_until(8);
      XBT_INFO("Triple the cost per element of the decimation method assigned to 'var3D'");
      ASSERT_NO_THROW(var->set_reduction_operation(decimator, {{"cost_per_element", "3"}}));
      XBT_INFO("Start a Transaction");
      engine->begin_transaction();
      XBT_INFO("Put reduced Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var));
      XBT_INFO("End a Transaction");
      engine->end_transaction();
      XBT_INFO("Sleep until t = 10s");
      sg4::this_actor::sleep_until(10);
      XBT_INFO("Create a second 3D variable");
      auto var2 = stream->define_variable("var3D_2", {640, 640, 640}, {0, 0, 0}, {640, 640, 640}, sizeof(double));
      XBT_INFO("Assign the decimation method to 'var3D_2'");
      ASSERT_NO_THROW(var2->set_reduction_operation(decimator, {{"stride", "2,2,2"}, {"interpolation", "quadratic"}}));
      XBT_INFO("Start a Transaction");
      engine->begin_transaction();
      XBT_INFO("Put reduced Variable 'var2' into the DTL");
      ASSERT_NO_THROW(engine->put(var2));
      XBT_INFO("End a Transaction");
      engine->end_transaction();
      XBT_INFO("Close the engine");
      engine->close();

      XBT_INFO("Get the name of the metadata file");
      auto metadata_file_name = stream->get_metadata_file_name();
      XBT_INFO("Check the contents of '%s'", metadata_file_name.c_str());
      std::ifstream file(metadata_file_name);
      ASSERT_TRUE(file.is_open());
      std::string file_contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

      file.close();
      const std::string& expected_contents =
          "8\tvar3D_2\t1*{640,640,640}\n"
          "  Transaction 4:\n"
          "    /host/scratch/my-working-dir/my-output/data.0: [0:320, 0:320, 0:320]\n"
          "8\tvar3D\t3*{640,640,640}\n"
          "  Transaction 1:\n"
          "    /host/scratch/my-working-dir/my-output/data.0: [0:640, 0:640, 0:640]\n"
          "  Transaction 2:\n"
          "    /host/scratch/my-working-dir/my-output/data.0: [0:640, 0:320, 0:160]\n"
          "  Transaction 3:\n"
          "    /host/scratch/my-working-dir/my-output/data.0: [0:640, 0:320, 0:160]\n";

      ASSERT_EQ(file_contents, expected_contents);
      std::remove(metadata_file_name.c_str());

      XBT_INFO("Disconnect the actor from the DTL");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLReductionTest, MultiPubDecimationFileEngine)
{
  DO_TEST_WITH_FORK([this]() {
    xbt_log_control_set("dtlmod_decimation_reduction.thresh:debug");
    this->setup_platform();

    for (long unsigned int i = 0; i < 2; i++) {
      host_->add_actor("pub" + std::to_string(i), [this, i]() {
        std::shared_ptr<dtlmod::ReductionMethod> decimator;
        auto dtl    = dtlmod::DTL::connect();
        auto stream = dtl->add_stream("my-output");
        stream->set_transport_method(dtlmod::Transport::Method::File);
        stream->set_engine_type(dtlmod::Engine::Type::File);
        XBT_INFO("Create a 2D-array variable with 20kx20k double, each publisher owns one half (along 2nd dimension)");
        auto var = stream->define_variable("var", {20000, 20000}, {0, 10000 * i}, {20000, 10000}, sizeof(double));
        XBT_INFO("Define a Decimation Reduction Method");
        ASSERT_NO_THROW(decimator = stream->define_reduction_method("decimation"));
        auto engine = stream->open("zone:my_fs:/host/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
        XBT_INFO("Wait for all publishers to have opened the stream");
        sg4::this_actor::sleep_for(.5);
        XBT_INFO("Assign the decimation method to 'var3D'");
        ASSERT_NO_THROW(var->set_reduction_operation(decimator, {{"stride", "2,2"}}));

        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Put Variable 'var' into the DTL");
        ASSERT_NO_THROW(engine->put(var));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());

        sg4::this_actor::sleep_for(1);
        XBT_INFO("Close the engine");
        ASSERT_NO_THROW(engine->close());
        XBT_INFO("Disconnect the actor");
        dtlmod::DTL::disconnect();
      });
    }

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLReductionTest, SinglePubSingleSubDecimationOnRead)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 2D-array variable with 20kx20k double");
      auto var    = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine = stream->open("zone:my_fs:/host/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Stream '%s' is ready for Publish data into the DTL", stream->get_cname());
      sg4::this_actor::sleep_for(1);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();

      XBT_INFO("Check that no Actor is connected to the DTL anymore");
      ASSERT_EQ(dtl->has_active_connections(), false);

      XBT_INFO("Wait until 10s before becoming a Subscriber");
      ASSERT_NO_THROW(sg4::this_actor::sleep_until(10));
      dtl = dtlmod::DTL::connect();

      XBT_INFO("Define a Decimation Reduction Method on Subscriber side");
      std::shared_ptr<dtlmod::ReductionMethod> decimator;
      ASSERT_NO_THROW(decimator = stream->define_reduction_method("decimation"));

      engine       = stream->open("zone:my_fs:/host/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");
      auto shape   = var_sub->get_shape();
      ASSERT_TRUE(var_sub->get_name() == "var");
      ASSERT_DOUBLE_EQ(var_sub->get_global_size(), 8. * 20000 * 20000);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Get the entire Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Assign the decimation method to 'var_sub'");
      ASSERT_NO_THROW(var_sub->set_reduction_operation(decimator, {{"stride", "2,2"}}));

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Get a subset of the Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());
      XBT_INFO("Check local size of var_sub. Should be 800,000,000 bytes");
      ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 10000 * 10000);

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
