/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <cmath>
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
    host_      = zone->add_host("host", "1Gf");
    disk_      = host_->add_disk("disk", "5.5GBps", "2.1GBps");
    zone->seal();

    auto my_fs = sgfs::FileSystem::create("my_fs");
    sgfs::FileSystem::register_file_system(zone, my_fs);
    auto local_storage = sgfs::OneDiskStorage::create("local_storage", disk_);
    my_fs->mount_partition("/host/scratch/", local_storage, "100MB");

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLReductionTest, SimpleDecimationFileEngine)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      // TODO do something
      std::shared_ptr<dtlmod::ReductionMethod> decimator;      
      XBT_INFO("Connect to the DTL");
      auto dtl = dtlmod::DTL::connect();
      XBT_INFO("Create a stream");
      auto stream = dtl->add_stream("Stream");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 3D variable");
      auto var = stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64},sizeof(double));
      XBT_INFO("Define a Decimation Reduction Method");
      ASSERT_NO_THROW(decimator = stream->define_reduction_method("decimation"));
      XBT_INFO("Assign the decimation method to 'var3D'");
      ASSERT_NO_THROW(var->set_reduction_operation(decimator, {{"stride", "1,2,4"}}));
      XBT_INFO("Open the stream in Pulish mode");
      auto engine = stream->open("zone:my_fs:/host/scratch/my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Start a Transaction");
      engine->begin_transaction();
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
      XBT_INFO("End a Transaction");
      XBT_INFO("Close the engine");
      engine->close();

      XBT_INFO("Disconnect the actor from the DTL");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
