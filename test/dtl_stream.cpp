/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>

#include <fsmod/FileSystem.hpp>
#include <fsmod/JBODStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_streams, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

class DTLStreamTest : public ::testing::Test {
public:
  DTLStreamTest() = default;
  sg4::Host* prod_host_;
  sg4::Host* cons_host_;

  void setup_platform()
  {
    auto* zone      = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_full("zone");
    prod_host_      = zone->create_host("prod_host", "1Gf")->set_core_count(2);
    cons_host_      = zone->create_host("cons_host", "1Gf");
    auto* prod_disk = prod_host_->create_disk("disk", "1kBps", "2kBps");
    auto* cons_disk = cons_host_->create_disk("disk", "1kBps", "2kBps");

    auto pfs_server = zone->create_host("pfs_server", "1Gf");
    std::vector<sg4::Disk*> pfs_disks;
    for (int i = 0; i < 4; i++)
      pfs_disks.push_back(pfs_server->create_disk("pfs_disk" + std::to_string(i), "200MBps", "100MBps"));
    auto remote_storage = sgfs::JBODStorage::create("pfs_storage", pfs_disks);
    remote_storage->set_raid_level(sgfs::JBODStorage::RAID::RAID5);

    const auto* prod_link = zone->create_link("prod_link", 120e6 / 0.97)->set_latency(0);
    const auto* cons_link = zone->create_link("cons_link", 120e6 / 0.97)->set_latency(0);
    zone->add_route(prod_host_, pfs_server, {prod_link});
    zone->add_route(cons_host_, pfs_server, {cons_link});
    zone->seal();

    auto fs = sgfs::FileSystem::create("fs");
    sgfs::FileSystem::register_file_system(zone, fs);
    fs->mount_partition("/pfs/", remote_storage, "100MB");

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLStreamTest, IncorrectStreamSettings)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestProducerActor", prod_host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> no_engine_type_stream;
      std::shared_ptr<dtlmod::Stream> no_transport_method_stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create the streams");
      ASSERT_NO_THROW(no_engine_type_stream = dtl->add_stream("no_engine_type_stream"));
      ASSERT_NO_THROW(no_engine_type_stream->set_transport_method(dtlmod::Transport::Method::File));
      ASSERT_NO_THROW(no_transport_method_stream = dtl->add_stream("nno_transport_method_stream"));
      ASSERT_NO_THROW(no_transport_method_stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Try to open the stream with no engine type set, which should fail");
      ASSERT_THROW(no_engine_type_stream->open("zone:fs:/pfs/file", dtlmod::Stream::Mode::Publish),
                   dtlmod::UndefinedEngineTypeException);
      XBT_INFO("Try to open the stream with no transport method set, which should fail");
      ASSERT_THROW(no_transport_method_stream->open("file", dtlmod::Stream::Mode::Publish),
                   dtlmod::UndefinedTransportMethodException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLStreamTest, PublishFileStreamOpenClose)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestProducerActor", prod_host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Engine> engine;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Set transport method to Transport::Method::File");
      ASSERT_NO_THROW(stream->set_transport_method(dtlmod::Transport::Method::File));
      XBT_INFO("Set engine to Engine::Type::File");
      ASSERT_NO_THROW(stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Open the Stream in Stream::Mode::Publish mode");
      ASSERT_NO_THROW(engine = stream->open("zone:fs:/pfs/file", dtlmod::Stream::Mode::Publish));
      XBT_INFO("Stream 1 is opened (%s, %s)", stream->get_engine_type_str(), stream->get_transport_method_str());
      ASSERT_TRUE(strcmp(stream->get_engine_type_str(), "Engine::Type::File") == 0);
      ASSERT_TRUE(strcmp(stream->get_transport_method_str(), "Transport::Method::File") == 0);
      XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
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

TEST_F(DTLStreamTest, PublishFileMultipleOpen)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestProducerActor", prod_host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Engine> engine;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      ASSERT_NO_THROW(stream->set_transport_method(dtlmod::Transport::Method::File));
      ASSERT_NO_THROW(stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Open the Stream in Stream::Mode::Publish mode");
      ASSERT_NO_THROW(engine = stream->open("zone:fs:/pfs/file", dtlmod::Stream::Mode::Publish));
      XBT_INFO("Check current number of publishers and subscribers");
      ASSERT_EQ(stream->get_num_publishers(), 1);
      ASSERT_EQ(stream->get_num_subscribers(), 0);
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    engine->add_actor("TestConsumerActor", cons_host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Engine> engine;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      ASSERT_NO_THROW(stream->set_transport_method(dtlmod::Transport::Method::File));
      ASSERT_NO_THROW(stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Open the Stream in Stream::Mode::Publish mode");
      ASSERT_NO_THROW(engine = stream->open("zone:fs:/pfs/file", dtlmod::Stream::Mode::Publish));
      XBT_INFO("Check current number of publishers and subscribers");
      ASSERT_EQ(stream->get_num_publishers(), 2);
      ASSERT_EQ(stream->get_num_subscribers(), 0);
      XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
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

TEST_F(DTLStreamTest, OpenWithRendezVous)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    for (int i = 0; i < 2; i++) {
      engine->add_actor("TestProducerActor_" + std::to_string(i), prod_host_, [this]() {
        std::shared_ptr<dtlmod::DTL> dtl;
        std::shared_ptr<dtlmod::Stream> stream;
        std::shared_ptr<dtlmod::Engine> engine;
        XBT_INFO("Connect to the DTL");
        ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
        XBT_INFO("Create a stream");
        ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
        ASSERT_NO_THROW(stream->set_engine_type(dtlmod::Engine::Type::Staging));
        ASSERT_NO_THROW(stream->set_transport_method(dtlmod::Transport::Method::MQ));
        XBT_INFO("Set this stream in rendez-vous mode");
        ASSERT_NO_THROW(stream->set_rendez_vous());
        XBT_INFO("Open the Stream in Stream::Mode::Publish mode");
        ASSERT_NO_THROW(engine = stream->open("foo", dtlmod::Stream::Mode::Publish));
        XBT_INFO("Open complete. Clock should be at 10s");
        ASSERT_DOUBLE_EQ(sg4::Engine::get_clock(), 10.0);
        XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
        XBT_INFO("Close the engine");
        ASSERT_NO_THROW(engine->close());
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
      });
    }

    engine->add_actor("TestConsumerActor", cons_host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Engine> engine;
      XBT_INFO("Let actor %s sleep for 10 seconds before anything", sg4::this_actor::get_cname());
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(10));
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Open the Stream in Stream::Mode::Subscribe mode, should unlock the publishers");
      ASSERT_NO_THROW(engine = stream->open("foo", dtlmod::Stream::Mode::Subscribe));
      XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
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
