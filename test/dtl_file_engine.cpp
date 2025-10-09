/* Copyright (c) 2024-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>
#include <cmath>


#include <fsmod/FileSystem.hpp>
#include <fsmod/FileSystemException.hpp>
#include <fsmod/JBODStorage.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_file_engine, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

class DTLFileEngineTest : public ::testing::Test {
public:
  DTLFileEngineTest() = default;

  void setup_platform()
  {
    sg4::NetZone* cluster = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_star("cluster");
    auto pfs_server       = cluster->add_host("pfs_server", "1Gf");
    std::vector<sg4::Disk*> pfs_disks;
    for (int i = 0; i < 4; i++)
      pfs_disks.push_back(pfs_server->add_disk("pfs_disk" + std::to_string(i), "2.5GBps", "1.2GBps"));
    auto remote_storage = sgfs::JBODStorage::create("pfs_storage", pfs_disks);
    remote_storage->set_raid_level(sgfs::JBODStorage::RAID::RAID5);

    std::vector<std::shared_ptr<sgfs::OneDiskStorage>> local_storages;
    for (int i = 0; i < 4; i++) {
      std::string hostname = std::string("node-") + std::to_string(i);
      auto* host           = cluster->add_host(hostname, "1Gf");
      sg4::Disk* disk      = host->add_disk(hostname + "_disk", "5.5GBps", "2.1GBps");
      local_storages.push_back(sgfs::OneDiskStorage::create(hostname + "_local_storage", disk));

      std::string linkname = std::string("link_") + std::to_string(i);
      auto* link_up        = cluster->add_link(linkname + "_UP", "1Gbps");
      auto* link_down      = cluster->add_link(linkname + "_DOWN", "1Gbps");
      auto* loopback =
          cluster->add_link(hostname + "_loopback", "10Gbps")->set_sharing_policy(sg4::Link::SharingPolicy::FATPIPE);

      cluster->add_route(host, nullptr, {sg4::LinkInRoute(link_up)}, false);
      cluster->add_route(nullptr, host, {sg4::LinkInRoute(link_down)}, false);
      cluster->add_route(host, host, {loopback});
    }

    cluster->seal();

    // Create a file system with multiple partitions: 1 per host and 1 Parallel file System
    auto my_fs = sgfs::FileSystem::create("my_fs");
    sgfs::FileSystem::register_file_system(cluster, my_fs);
    my_fs->mount_partition("/pfs/", remote_storage, "500TB");
    for (int i = 0; i < 4; i++) {
      std::string partition_name = std::string("/node-") + std::to_string(i) + "/scratch/";
      my_fs->mount_partition(partition_name, local_storages.at(i), "1TB");
    }

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLFileEngineTest, BogusStoragePaths)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    sg4::Host::by_name("node-0")->add_actor("TestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Try to open the stream with a badly formatted first argument");
      ASSERT_THROW(stream->open("/node-0/scratch/my-working-dir/my-output", 
                   dtlmod::Stream::Mode::Publish), dtlmod::IncorrectPathDefinitionException);
      XBT_INFO("Try to open the stream with a bogus NetZone name");
      ASSERT_THROW(stream->open("bogus_zone:my_fs:/node-0/scratch/my-working-dir/my-output", 
                   dtlmod::Stream::Mode::Publish), dtlmod::IncorrectPathDefinitionException);
      XBT_INFO("Try to open the stream with a bogus file system name");
      ASSERT_THROW(stream->open("cluster:bogus_fs:/node-0/scratch/my-working-dir/my-output", 
                   dtlmod::Stream::Mode::Publish), dtlmod::IncorrectPathDefinitionException);
      XBT_INFO("Try to open the stream with a bogus partition name");
      ASSERT_THROW(stream->open("cluster:my_fs:/bogus_partition/my-working-dir/my-output", 
                   dtlmod::Stream::Mode::Publish), dtlmod::IncorrectPathDefinitionException);
      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
      });
    XBT_INFO("Simulation will fail as the Stream::open throws an exception from a critical section");
    ASSERT_DEATH(sg4::Engine::get_instance()->run(), ".*");
  });
}

TEST_F(DTLFileEngineTest, SinglePublisherLocalStorage)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    sg4::Host::by_name("node-0")->add_actor("TestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 2D-array variable with 20kx20k double");
      auto var = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine =
          stream->open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Stream '%s' (Engine '%s') is ready for Publish data into the DTL", stream->get_cname(), 
               engine->get_cname());
      for (int i = 0; i < 5; i++) {
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Put Variable 'var' into the DTL");
        ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());
      }
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      auto file_system =
          sgfs::FileSystem::get_file_systems_by_netzone(sg4::Engine::get_instance()->netzone_by_name_or_null("cluster"))
              .at("my_fs");
      std::string dirname = "/node-0/scratch/my-working-dir/my-output";
      auto file_list      = file_system->list_files_in_directory(dirname);
      XBT_INFO("List contents of %s", dirname.c_str());
      for (const auto& filename : file_list) {
        XBT_INFO(" - %s of size %llu", filename.c_str(), file_system->file_size(dirname + "/" + filename));
      }
      XBT_INFO("Check the size of the produced file. Should be 5*(20k*20k*8)");
      ASSERT_DOUBLE_EQ(file_system->file_size("/node-0/scratch/my-working-dir/my-output/data.0"),
                       5. * 8 * 20000 * 20000);
      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLFileEngineTest, SinglePubSingleSubLocalStorage)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    sg4::Host::by_name("node-0")->add_actor("TestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 2D-array variable with 20kx20k double");
      auto var = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine =
          stream->open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Stream '%s' is ready for Publish data into the DTL", stream->get_cname());
      sg4::this_actor::sleep_for(1);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
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
      dtl    = dtlmod::DTL::connect();
      engine = stream->open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Subscribe);
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

      XBT_INFO("Set a selection for 'var_sub': Just get the second half of the first dimension");
      ASSERT_NO_THROW(var_sub->set_selection({10000, 0}, {10000, shape[1]}));

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Get a subset of the Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());
      XBT_INFO("Check local size of var_sub. Should be 1,600,000,000 bytes");
      ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 10000 * 20000);

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLFileEngineTest, MultiplePubSingleSubSharedStorage)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    std::vector<sg4::Host*> pub_hosts = {sg4::Host::by_name("node-0"), sg4::Host::by_name("node-1")};
    auto* sub_host                    = sg4::Host::by_name("node-2");

    for (long unsigned int i = 0; i < 2; i++) {
      pub_hosts[i]->add_actor(pub_hosts[i]->get_name() + "_pub", [this, i]() {
        auto dtl    = dtlmod::DTL::connect();
        auto stream = dtl->add_stream("my-output");
        stream->set_transport_method(dtlmod::Transport::Method::File);
        stream->set_engine_type(dtlmod::Engine::Type::File);
        XBT_INFO("Create a 2D-array variable with 20kx20k double, each publisher owns one half (along 2nd dimension)");
        auto var    = stream->define_variable("var", {20000, 20000}, {0, 10000 * i}, {20000, 10000}, sizeof(double));
        auto engine = stream->open("cluster:my_fs:/pfs/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
        XBT_INFO("Wait for all publishers to have opened the stream");
        sg4::this_actor::sleep_for(.5);
        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Put Variable 'var' into the DTL");
        ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());

        sg4::this_actor::sleep_for(1);
        XBT_INFO("Close the engine");
        ASSERT_NO_THROW(engine->close());
        XBT_INFO("Disconnect the actor");
        dtlmod::DTL::disconnect();
      });
    }

    sub_host->add_actor("node-2_sub", [this]() {
      auto dtl = dtlmod::DTL::connect();
      XBT_INFO("Wait for 10s before becoming a Subscriber. It's not enough for the publishers to finish writing");
      ASSERT_NO_THROW(sg4::this_actor::sleep_for(10));
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/pfs/my-working-dir/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Transition can start as publishers stored metadata");
      ASSERT_DOUBLE_EQ(sg4::Engine::get_clock(), 10);
      XBT_INFO("Get the entire Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());
      XBT_INFO("Check local size of var_sub. Should be 3,200,000,000 bytes");
      ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 20000 * 20000);
      ASSERT_DOUBLE_EQ(std::round(sg4::Engine::get_clock() * 1e6) / 1e6, 42.469851);
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLFileEngineTest, SinglePubMultipleSubSharedStorage)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* pub_host                    = sg4::Host::by_name("node-0");
    std::vector<sg4::Host*> sub_hosts = {sg4::Host::by_name("node-1"), sg4::Host::by_name("node-2")};

    pub_host->add_actor("node-0_pub",  [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 2D-array variable with 10kx10k double");
      auto var    = stream->define_variable("var", {10000, 10000}, {0, 0}, {10000, 10000}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/pfs/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Wait for all publishers to have opened the stream");
      sg4::this_actor::sleep_for(.5);
      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
      XBT_INFO("End a Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      sg4::this_actor::sleep_for(1);
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    for (long unsigned int i = 0; i < 2; i++) {
      sub_hosts[i]->add_actor(sub_hosts[i]->get_name() + "_sub", [this, i]() {
        auto dtl = dtlmod::DTL::connect();
        XBT_INFO("Wait for 10s before becoming a Subscriber");
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(10));
        auto stream  = dtl->add_stream("my-output");
        auto engine  = stream->open("cluster:my_fs:/pfs/my-working-dir/my-output", dtlmod::Stream::Mode::Subscribe);
        auto var_sub = stream->inquire_variable("var");
        XBT_INFO("Each publisher selects one half (along 2nd dimension) of the Variable 'var' from the DTL");
        ASSERT_NO_THROW(var_sub->set_selection({0, 5000 * i}, {10000, 5000}));
        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Get the selection");
        ASSERT_NO_THROW(engine->get(var_sub));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());
        XBT_INFO("Check local size of var_sub. Should be 400,000,000 bytes");
        ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 10000 * 5000);

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

TEST_F(DTLFileEngineTest, SetTransactionSelection)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    sg4::Host::by_name("node-0")->add_actor("TestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      XBT_INFO("Create a 2D-array variable with 20kx20k double  and publish it in 5 transactions");
      auto var = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine =
          stream->open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Publish);
      for (int i = 0; i < 5; i++) {
        engine->begin_transaction();
        engine->put(var, var->get_local_size());
        engine->end_transaction();
        sg4::this_actor::sleep_for(1);
      }
      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      dtlmod::DTL::disconnect();

      XBT_INFO("Check that no Actor is connected to the DTL anymore");
      ASSERT_EQ(dtl->has_active_connections(), false);

      XBT_INFO("Wait until 10s before becoming a Subscriber");
      ASSERT_NO_THROW(sg4::this_actor::sleep_until(10));
      dtl    = dtlmod::DTL::connect();
      engine = stream->open("cluster:my_fs:/node-0/scratch/my-working-dir/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");
      XBT_INFO("Select transaction #1 (the second one, 0 is first)");
      ASSERT_NO_THROW(var_sub->set_transaction_selection(1));
      engine->begin_transaction();
      ASSERT_NO_THROW(engine->get(var_sub));
      engine->end_transaction();
      XBT_INFO("Select transaction #4 (the last one)");
      ASSERT_NO_THROW(var_sub->set_transaction_selection(4));
      engine->begin_transaction();
      engine->get(var_sub);
      engine->end_transaction();
      XBT_INFO("Check local size of var_sub. Should be 3,200,000,000 bytes");
      ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 20000 * 20000);

      XBT_INFO("Select transactions #2 and #3 (2 transactions from #2)");
      ASSERT_NO_THROW(var_sub->set_transaction_selection(2, 2));
      engine->begin_transaction();
      engine->get(var_sub);
      engine->end_transaction();
      XBT_INFO("Check local size of var_sub. Should be 6,400,000,000 bytes as we store 2 steps");
      ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 2 * 8. * 20000 * 20000);

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
