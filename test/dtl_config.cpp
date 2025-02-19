/* Copyright (c) 2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>

#include <fsmod/FileSystem.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_config, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

class DTLConfigTest : public ::testing::Test {
public:
  DTLConfigTest() = default;
  sg4::Host* host_;

  void setup_platform()
  {
    auto* root = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_full("root");
    host_      = root->create_host("host", "1Gf");
    auto* disk = host_->create_disk("disk", "1kBps", "2kBps");
    root->seal();

    auto local_storage = sgfs::OneDiskStorage::create("local_storage", disk);
    auto fs            = sgfs::FileSystem::create("fs");
    sgfs::FileSystem::register_file_system(root, fs);
    fs->mount_partition("/scratch/", local_storage, "100MB");

    // Create the DTL with its configuration file
    dtlmod::DTL::create("./config_files/test/DTL-config.json");
  }
};

TEST_F(DTLConfigTest, ConfigFile)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Engine> engine;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream1"));
      XBT_INFO("Open the stream");
      ASSERT_NO_THROW(engine = stream->open("root:fs:/scratch/file", dtlmod::Stream::Mode::Publish));
      XBT_INFO("Stream 1 is opened (%s, %s)", stream->get_engine_type_str(), stream->get_transport_method_str());
      ASSERT_TRUE(strcmp(stream->get_engine_type_str(), "Engine::Type::File") == 0);
      ASSERT_TRUE(strcmp(stream->get_transport_method_str(), "Transport::Method::File") == 0);
      XBT_INFO("Let the actor sleep for 1 second");
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream2"));
      XBT_INFO("Open the stream");
      ASSERT_NO_THROW(engine = stream->open("staging", dtlmod::Stream::Mode::Publish));
      XBT_INFO("Stream 2 is opened (%s, %s)", stream->get_engine_type_str(), stream->get_transport_method_str());
      ASSERT_TRUE(strcmp(stream->get_engine_type_str(), "Engine::Type::Staging") == 0);
      ASSERT_TRUE(strcmp(stream->get_transport_method_str(), "Transport::Method::MQ") == 0);
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}
