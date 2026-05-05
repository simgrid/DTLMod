/* Copyright (c) 2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>

#include <fsmod/FileSystem.hpp>
#include <fsmod/JBODStorage.hpp>
#include <fsmod/OneDiskStorage.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_test_cancel, "Logging category for this dtlmod test");

namespace sg4  = simgrid::s4u;
namespace sgfs = simgrid::fsmod;

class DTLCancelTest : public ::testing::Test {
public:
  DTLCancelTest() = default;

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

  void setup_slow_staging_platform()
  {
    auto* root         = sg4::Engine::get_instance()->get_netzone_root();
    auto* internet     = root->add_link("internet", "1MBps")->set_latency("1ms");
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
};

// Publisher is stuck in begin_transaction() waiting for a subscriber that never shows up.
// An external canceller fires after 0.5s, unblocking the publisher with TransactioncanceledException.
// The subscriber registers but sleeps past the cancellation point, then gets TransactioncanceledException
// immediately on its own begin_transaction() because canceled_ is already true.
TEST_F(DTLCancelTest, CancelStagingTransaction_WaitingForSubscriber_MQ)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    auto* pub_host  = sg4::Host::by_name("host-0.prod");
    auto* sub_host  = sg4::Host::by_name("host-0.cons");
    auto* wdog_host = sg4::Host::by_name("host-1.prod");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::MQ);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      XBT_INFO("Begin transaction (will block waiting for subscriber)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Publisher caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      sg4::this_actor::sleep_for(2.0); // sleep past the cancellation point
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Subscriber caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Same scenario with Mailbox transport.
TEST_F(DTLCancelTest, CancelStagingTransaction_WaitingForSubscriber_Mailbox)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    auto* pub_host  = sg4::Host::by_name("host-0.prod");
    auto* sub_host  = sg4::Host::by_name("host-0.cons");
    auto* wdog_host = sg4::Host::by_name("host-1.prod");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::Mailbox);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      XBT_INFO("Begin transaction (will block waiting for subscriber)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Publisher caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      sg4::this_actor::sleep_for(2.0);
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Subscriber caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Subscriber is stuck in begin_transaction() waiting for the publisher to start a transaction.
// Publisher opens the stream but never calls begin_transaction().
// Canceller fires after 0.5s, unblocking the subscriber.
// Publisher then gets TransactioncanceledException immediately on its begin_transaction().
TEST_F(DTLCancelTest, CancelStagingTransaction_WaitingForPublisher_MQ)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_staging_platform();
    auto* pub_host  = sg4::Host::by_name("host-0.prod");
    auto* sub_host  = sg4::Host::by_name("host-0.cons");
    auto* wdog_host = sg4::Host::by_name("host-1.prod");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::MQ);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      sg4::this_actor::sleep_for(2.0); // sleep past the cancellation point
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Publisher caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Begin transaction (will block waiting for publisher to start a transaction)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Subscriber caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// FileEngine: subscriber is stuck in begin_transaction() waiting for the publisher to complete a transaction.
// Publisher opens the stream and registers but never calls begin_transaction().
// Canceller fires after 0.5s, unblocking the subscriber.
TEST_F(DTLCancelTest, CancelFileEngineTransaction_WaitingForPublisher)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_file_platform();
    auto* pub_host  = sg4::Host::by_name("node-0");
    auto* sub_host  = sg4::Host::by_name("node-1");
    auto* wdog_host = sg4::Host::by_name("node-2");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      sg4::this_actor::sleep_for(2.0); // sleep past the cancellation point
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Publisher caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Begin transaction (will block waiting for publisher to complete a transaction)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Subscriber caught TransactioncanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher and subscriber are both engaged in a long Mailbox transfer (Mailbox simulates bandwidth; MQ does not).
// Publisher completes T1 end_transaction() (starting slow async Comms) then blocks in T2 begin_transaction()
// waiting for T1 sends to complete. Subscriber blocks in T1 end_transaction() waiting for receives.
// Canceller fires after 0.5s, unblocking both with TransactioncanceledException.
TEST_F(DTLCancelTest, CancelStagingTransaction_MidTransaction_Mailbox)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_staging_platform();
    auto* pub_host  = sg4::Host::by_name("host-0.prod");
    auto* sub_host  = sg4::Host::by_name("host-0.cons");
    auto* wdog_host = sg4::Host::by_name("host-1.prod");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::Mailbox);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      // T1: completes normally, starting slow async Comms over the 1MBps internet link
      engine->begin_transaction();
      engine->put(var);
      engine->end_transaction();

      // T2: blocks waiting for T1 Comms to drain -- canceled mid-transfer
      XBT_INFO("Begin T2 (will block waiting for T1 sends to complete over slow link)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Publisher caught TransactioncanceledException in T2 begin_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      // T1: blocks in end_transaction() waiting for slow receives -- canceled mid-transfer
      engine->begin_transaction();
      engine->get(var_sub);
      XBT_INFO("End T1 (will block waiting for receives over slow link)");
      ASSERT_THROW(engine->end_transaction(), dtlmod::TransactioncanceledException);
      XBT_INFO("Subscriber caught TransactioncanceledException in T1 end_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
