/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

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

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_test_stream, "Logging category for this dtlmod test");

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
    prod_host_      = zone->add_host("prod_host", "1Gf")->set_core_count(2);
    cons_host_      = zone->add_host("cons_host", "1Gf");
    prod_host_->add_disk("disk", "1kBps", "2kBps");
    cons_host_->add_disk("disk", "1kBps", "2kBps");

    auto pfs_server = zone->add_host("pfs_server", "1Gf");
    std::vector<sg4::Disk*> pfs_disks;
    for (int i = 0; i < 4; i++)
      pfs_disks.push_back(pfs_server->add_disk("pfs_disk" + std::to_string(i), "200MBps", "100MBps"));
    auto remote_storage = sgfs::JBODStorage::create("pfs_storage", pfs_disks);
    remote_storage->set_raid_level(sgfs::JBODStorage::RAID::RAID5);

    const auto* prod_link = zone->add_link("prod_link", 120e6 / 0.97)->set_latency(0);
    const auto* cons_link = zone->add_link("cons_link", 120e6 / 0.97)->set_latency(0);
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
    prod_host_->add_actor("TestProducerActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> no_engine_type_stream;
      std::shared_ptr<dtlmod::Stream> no_transport_method_stream;
      std::shared_ptr<dtlmod::Stream> file_transport_with_staging_engine_stream;
      std::shared_ptr<dtlmod::Stream> file_engine_with_mq_transport_stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());

      ASSERT_NO_THROW(no_engine_type_stream = dtl->add_stream("no_engine_type_stream"));
      ASSERT_NO_THROW(no_engine_type_stream->set_transport_method(dtlmod::Transport::Method::File));
      XBT_INFO("Try to open the stream with no engine type set, which should fail");
      ASSERT_THROW((void)no_engine_type_stream->open("zone:fs:/pfs/file", dtlmod::Stream::Mode::Publish),
                   dtlmod::UndefinedEngineTypeException);

      ASSERT_NO_THROW(no_transport_method_stream = dtl->add_stream("no_transport_method_stream"));
      ASSERT_NO_THROW(no_transport_method_stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Try to open the stream with no transport method set, which should fail");
      ASSERT_THROW((void)no_transport_method_stream->open("file", dtlmod::Stream::Mode::Publish),
                   dtlmod::UndefinedTransportMethodException);

      ASSERT_NO_THROW(file_engine_with_mq_transport_stream = dtl->add_stream("file_engine_with_mq_transport_stream"));
      ASSERT_NO_THROW(file_engine_with_mq_transport_stream->set_engine_type(dtlmod::Engine::Type::File));
      XBT_INFO("Try to set a invalid transport method for this engine type");
      ASSERT_THROW(file_engine_with_mq_transport_stream->set_transport_method(dtlmod::Transport::Method::MQ),
                   dtlmod::InvalidEngineAndTransportCombinationException);

      auto mq_transport_with_file_engine = dtl->add_stream("mq_transport_with_file_engine");
      mq_transport_with_file_engine->set_transport_method(dtlmod::Transport::Method::MQ);
      XBT_INFO("Try to set a invalid engine type for this transport method");
      ASSERT_THROW(mq_transport_with_file_engine->set_engine_type(dtlmod::Engine::Type::File),
                   dtlmod::InvalidEngineAndTransportCombinationException);

      ASSERT_NO_THROW(file_transport_with_staging_engine_stream =
                          dtl->add_stream("file_transport_with_staging_engine_stream"));
      ASSERT_NO_THROW(file_transport_with_staging_engine_stream->set_transport_method(dtlmod::Transport::Method::File));
      XBT_INFO("Try to set a invalid engine type for this transport method");
      ASSERT_THROW(file_transport_with_staging_engine_stream->set_engine_type(dtlmod::Engine::Type::Staging),
                   dtlmod::InvalidEngineAndTransportCombinationException);

      auto staging_engine_with_file_transport_stream = dtl->add_stream("staging_engine_with_file_transport_stream");
      staging_engine_with_file_transport_stream->set_engine_type(dtlmod::Engine::Type::Staging);
      XBT_INFO("Try to set a invalid transport method for this engine type");
      ASSERT_THROW(staging_engine_with_file_transport_stream->set_transport_method(dtlmod::Transport::Method::File),
                   dtlmod::InvalidEngineAndTransportCombinationException);

      auto unknown = dtl->add_stream("unknown");
      ASSERT_THROW(unknown->set_engine_type(static_cast<dtlmod::Engine::Type>(4)), dtlmod::UnknownEngineTypeException);
      ASSERT_THROW(unknown->set_transport_method(static_cast<dtlmod::Transport::Method>(4)),
                   dtlmod::UnknownTransportMethodException);
      auto multiple = dtl->add_stream("multiple");
      multiple->set_engine_type(dtlmod::Engine::Type::Staging);
      ASSERT_THROW(multiple->set_engine_type(dtlmod::Engine::Type::File), dtlmod::MultipleEngineTypeException);
      multiple->set_transport_method(dtlmod::Transport::Method::MQ);
      ASSERT_THROW(multiple->set_transport_method(dtlmod::Transport::Method::Mailbox),
                   dtlmod::MultipleTransportMethodException);
      ASSERT_THROW((void)multiple->open("file", static_cast<dtlmod::Stream::Mode>(2)),
                   dtlmod::UnknownOpenModeException);

      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLStreamTest, PublishFileStreamOpenClose)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    prod_host_->add_actor("TestProducerActor", [this]() {
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
      XBT_INFO("Stream is opened (%s, %s)", stream->get_engine_type_str().value_or("Unknown"),
               stream->get_transport_method_str().value_or("Unknown"));
      ASSERT_TRUE(strcmp(stream->get_engine_type_str().value(), "Engine::Type::File") == 0);
      ASSERT_TRUE(strcmp(stream->get_transport_method_str().value(), "Transport::Method::File") == 0);
      XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
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

TEST_F(DTLStreamTest, PublishFileMultipleOpen)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    prod_host_->add_actor("TestProducerActor", [this]() {
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
      ASSERT_EQ(stream->get_num_publishers(), 1U);
      ASSERT_EQ(stream->get_num_subscribers(), 0U);
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    cons_host_->add_actor("TestConsumerActor", [this]() {
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
      ASSERT_EQ(stream->get_num_publishers(), 2U);
      ASSERT_EQ(stream->get_num_subscribers(), 0U);
      XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
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
