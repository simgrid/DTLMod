/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>

#include <array>

#include <fsmod/FileSystem.hpp>
#include <fsmod/JBODStorage.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_test_eos, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

// End-of-stream: once every publisher has closed its engine, a subscriber that asks for a transaction which was never
// produced must be released with an EndOfStreamException (instead of blocking forever). This is what lets a consumer
// terminate when its upstream stops, rather than hang. Each test has a publisher produce a fixed number of
// transactions and then close; the subscriber loops until it catches the exception.
//
// The per-subscriber outcome (how many transactions were read, whether end-of-stream was reached) is recorded in
// variables captured by reference and asserted AFTER Engine::run() returns -- never inside the subscriber actor. If
// the mechanism were missing the subscriber would deadlock, run() would return with the actor still blocked, and an
// in-actor assertion would simply never execute (a false pass). Asserting after run() turns that hang into a failure.
class DTLEndOfStreamTest : public ::testing::Test {
public:
  DTLEndOfStreamTest() = default;

  sg4::NetZone* add_cluster(sg4::NetZone* root, const std::string& suffix, const int num_hosts)
  {
    auto* cluster = root->add_netzone_star("cluster" + suffix);
    cluster->set_gateway(cluster->add_router("cluster" + suffix + "-router"));
    auto* backbone = cluster->add_link("backbone" + suffix, "100Gbps")->set_latency("100us");
    for (int i = 0; i < num_hosts; i++) {
      std::string name = "host-" + std::to_string(i) + suffix;
      const auto* host = cluster->add_host(name, "1Gf");
      const auto* link = cluster->add_link(name + "_link", "10Gbps")->set_latency("10us");
      cluster->add_route(host, nullptr, {link, backbone});
    }
    cluster->seal();
    return cluster;
  }

  void setup_staging_platform()
  {
    auto* root         = sg4::Engine::get_instance()->get_netzone_root();
    auto* internet     = root->add_link("internet", "500MBps")->set_latency("1ms");
    auto* prod_cluster = add_cluster(root, ".prod", 4);
    auto* cons_cluster = add_cluster(root, ".cons", 4);
    root->add_route(prod_cluster, cons_cluster, {internet});
    root->seal();
    dtlmod::DTL::create();
  }

  void setup_file_platform()
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
      std::string hostname = "node-" + std::to_string(i);
      auto* host           = cluster->add_host(hostname, "1Gf");
      auto* disk           = host->add_disk(hostname + "_disk", "5.5GBps", "2.1GBps");
      local_storages.push_back(sgfs::OneDiskStorage::create(hostname + "_local_storage", disk));
      std::string linkname = "link_" + std::to_string(i);
      auto* link_up        = cluster->add_link(linkname + "_UP", "1Gbps");
      auto* link_down      = cluster->add_link(linkname + "_DOWN", "1Gbps");
      auto* loopback =
          cluster->add_link(hostname + "_loopback", "10Gbps")->set_sharing_policy(sg4::Link::SharingPolicy::FATPIPE);
      cluster->add_route(host, nullptr, {sg4::LinkInRoute(link_up)}, false);
      cluster->add_route(nullptr, host, {sg4::LinkInRoute(link_down)}, false);
      cluster->add_route(host, host, {loopback});
    }
    cluster->seal();

    auto my_fs = sgfs::FileSystem::create("my_fs");
    sgfs::FileSystem::register_file_system(cluster, my_fs);
    my_fs->mount_partition("/pfs/", remote_storage, "500TB");
    for (int i = 0; i < 4; i++)
      my_fs->mount_partition("/node-" + std::to_string(i) + "/scratch/", local_storages.at(i), "1TB");

    dtlmod::DTL::create();
  }

  // Publisher actor body shared by the tests: produce n_tx transactions then close.
  static void publish_n(dtlmod::Engine::Type type, dtlmod::Transport::Method method, const std::string& engine_name,
                        int n_tx)
  {
    auto dtl    = dtlmod::DTL::connect();
    auto stream = dtl->add_stream("my-output");
    stream->set_engine_type(type);
    stream->set_transport_method(method);
    auto var    = stream->define_variable("var", {100, 100}, {0, 0}, {100, 100}, sizeof(double));
    auto engine = stream->open(engine_name, dtlmod::Stream::Mode::Publish);
    for (int i = 0; i < n_tx; i++) {
      engine->begin_transaction();
      engine->put(var);
      engine->end_transaction();
    }
    engine->close();
    dtlmod::DTL::disconnect();
  }

  // Subscriber actor body: read until end-of-stream, recording the outcome through the referenced variables.
  static void consume_until_eos(const std::string& engine_name, int& reads, bool& eos)
  {
    auto dtl     = dtlmod::DTL::connect();
    auto stream  = dtl->add_stream("my-output");
    auto engine  = stream->open(engine_name, dtlmod::Stream::Mode::Subscribe);
    auto var_sub = stream->inquire_variable("var");
    var_sub->set_selection({0, 0}, {100, 100});
    try {
      while (true) {
        engine->begin_transaction();
        engine->get(var_sub);
        engine->end_transaction();
        reads++;
      }
    } catch (const dtlmod::EndOfStreamException&) {
      eos = true;
    }
    engine->close();
    dtlmod::DTL::disconnect();
  }
};

TEST_F(DTLEndOfStreamTest, StagingSingleSubscriber_MQ)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    int reads = 0;
    bool eos  = false;
    sg4::Host::by_name("host-0.prod")->add_actor("Pub", []() {
      publish_n(dtlmod::Engine::Type::Staging, dtlmod::Transport::Method::MQ, "my-output", 2);
    });
    sg4::Host::by_name("host-0.cons")->add_actor("Sub", [&reads, &eos]() {
      consume_until_eos("my-output", reads, eos);
    });
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    ASSERT_TRUE(eos);
    ASSERT_EQ(reads, 2);
  });
}

TEST_F(DTLEndOfStreamTest, StagingSingleSubscriber_Mailbox)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    int reads = 0;
    bool eos  = false;
    sg4::Host::by_name("host-0.prod")->add_actor("Pub", []() {
      publish_n(dtlmod::Engine::Type::Staging, dtlmod::Transport::Method::Mailbox, "my-output", 3);
    });
    sg4::Host::by_name("host-0.cons")->add_actor("Sub", [&reads, &eos]() {
      consume_until_eos("my-output", reads, eos);
    });
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    ASSERT_TRUE(eos);
    ASSERT_EQ(reads, 3);
  });
}

// Two subscribers sharing the same Staging engine must both reach end-of-stream. This is the case that exercises the
// per-subscriber rollback of num_subscribers_starting_ on the EOS throw: an imbalance there would desynchronize the
// publisher/subscriber rendez-vous and either deadlock or crash.
TEST_F(DTLEndOfStreamTest, StagingMultipleSubscribers_MQ)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    std::array<int, 2> reads = {0, 0};
    std::array<bool, 2> eos  = {false, false};
    sg4::Host::by_name("host-0.prod")->add_actor("Pub", []() {
      publish_n(dtlmod::Engine::Type::Staging, dtlmod::Transport::Method::MQ, "my-output", 2);
    });
    for (int s = 0; s < 2; s++)
      sg4::Host::by_name("host-" + std::to_string(s) + ".cons")
          ->add_actor("Sub" + std::to_string(s),
                      [&reads, &eos, s]() { consume_until_eos("my-output", reads[s], eos[s]); });
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    for (int s = 0; s < 2; s++) {
      ASSERT_TRUE(eos[s]) << "subscriber " << s << " did not reach end of stream";
      ASSERT_EQ(reads[s], 2) << "subscriber " << s << " read a wrong number of transactions";
    }
  });
}

TEST_F(DTLEndOfStreamTest, FileEngineSingleSubscriber)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_file_platform();
    int reads                     = 0;
    bool eos                      = false;
    const std::string engine_name = "cluster:my_fs:/node-0/scratch/my-output";
    sg4::Host::by_name("node-0")->add_actor("Pub", [engine_name]() {
      publish_n(dtlmod::Engine::Type::File, dtlmod::Transport::Method::File, engine_name, 2);
    });
    sg4::Host::by_name("node-1")->add_actor(
        "Sub", [&reads, &eos, engine_name]() { consume_until_eos(engine_name, reads, eos); });
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    ASSERT_TRUE(eos);
    ASSERT_EQ(reads, 2);
  });
}
