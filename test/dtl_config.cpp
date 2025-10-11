/* Copyright (c) 2024-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <cstdio>

#include <fsmod/FileSystem.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/version.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

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
    host_      = root->add_host("host", "1Gf");
    auto* disk = host_->add_disk("disk", "1kBps", "2kBps");
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
    int ver_major;
    int ver_minor;
    int ver_patch;
    ASSERT_NO_THROW(dtlmod_version_get(&ver_major, &ver_minor, &ver_patch));
    XBT_INFO("Using DTLMod v%d.%d.%d",ver_major, ver_minor, ver_patch);

    host_->add_actor("TestActor", [this]() {
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
      XBT_INFO("Check if this stream is set to export metadata (it is)");
      ASSERT_TRUE(stream->does_export_metadata());
      XBT_INFO("Change the metadata export setting and check again");
      ASSERT_NO_THROW(stream->unset_metadata_export());
      ASSERT_FALSE(stream->does_export_metadata());
      XBT_INFO("Let the actor sleep for 1 second");
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Get a stream by name from the DTL");
      ASSERT_NO_THROW(stream = dtl->get_stream_by_name_or_null("Stream2"));
      XBT_INFO("Try to get a stream by name from the DTL that doesn't exist. Should return nullptr");
      ASSERT_EQ(dtl->get_stream_by_name_or_null("Unknown Stream"), nullptr);
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
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLConfigTest, BogusConfigFile)
{
  DO_TEST_WITH_FORK([this]() {
    auto* root = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_full("root");
    auto host  = root->add_host("host", "1Gf");
    root->seal();

    std::string bogus_engine_json =
      R"({"streams": [ { "name": "Stream1", "engine": { "type": "Whatever", "transport_method": "File" } } ] })";
    std::ofstream tmp_file("./bogus_engine.json");
    tmp_file << bogus_engine_json;
    tmp_file.close();
    ASSERT_THROW(dtlmod::DTL::create("./bogus_engine.json"), dtlmod::UnknownEngineTypeException) ;
    std::remove("./bogus_engine.json");

    std::string bogus_transport_json =
       R"({"streams": [ { "name": "Stream1", "engine": { "type": "File", "transport_method": "Whatever" } } ] })";
    std::ofstream tmp_file2("./bogus_transport.json");
    tmp_file2 << bogus_transport_json;
    tmp_file2.close();
    ASSERT_THROW(dtlmod::DTL::create("./bogus_transport.json"), dtlmod::UnknownTransportMethodException) ;
    std::remove("./bogus_transport.json");
  });
}
