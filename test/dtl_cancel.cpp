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
#include <simgrid/s4u/Io.hpp>

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

  void setup_slow_file_platform()
  {
    sg4::NetZone* cluster = sg4::Engine::get_instance()->get_netzone_root()->add_netzone_star("cluster");
    auto pfs_server       = cluster->add_host("pfs_server", "1Gf");
    std::vector<sg4::Disk*> pfs_disks;
    for (int i = 0; i < 4; i++)
      pfs_disks.push_back(pfs_server->add_disk("pfs_disk" + std::to_string(i), "1MBps", "1MBps"));
    auto remote_storage = sgfs::JBODStorage::create("pfs_storage", pfs_disks);
    remote_storage->set_raid_level(sgfs::JBODStorage::RAID::RAID5);

    std::vector<std::shared_ptr<sgfs::OneDiskStorage>> local_storages;
    for (int i = 0; i < 4; i++) {
      std::string hostname = "node-" + std::to_string(i);
      auto* host           = cluster->add_host(hostname, "1Gf");
      auto* disk           = host->add_disk(hostname + "_disk", "1MBps", "1MBps");
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
// An external canceller fires after 0.5s, unblocking the publisher with TransactionCanceledException.
// The subscriber registers but sleeps past the cancellation point, then gets TransactionCanceledException
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
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      sg4::this_actor::sleep_for(2.0); // sleep past the cancellation point
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
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
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      sg4::this_actor::sleep_for(2.0);
      XBT_INFO("Begin transaction (canceled_ already true)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Subscriber is stuck in begin_transaction() waiting for the publisher to start a transaction.
// Publisher opens the stream but never calls begin_transaction().
// Canceller fires after 0.5s, unblocking the subscriber.
// Publisher then gets TransactionCanceledException immediately on its begin_transaction().
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
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Begin transaction (will block waiting for publisher to start a transaction)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
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
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Begin transaction (will block waiting for publisher to complete a transaction)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Both publisher and subscriber complete two transactions before the canceller fires.
// cancel_transaction(0) is a no-op because both sides have already advanced past transaction 0
// (i.e., both current_pub_transaction_id_ and current_sub_transaction_id_ are > 0).
// This exercises the get_current_sub_transaction_impl() path in the no-op guard of
// Engine::cancel_transaction() for the StagingEngine.
TEST_F(DTLCancelTest, CancelNoOp_BothSidesPastTransaction_StagingMQ)
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
      auto var    = stream->define_variable("var", {100}, {0}, {100}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Both sides completed T1 and T2 at t=0; cancel_transaction(0) must be a no-op");
        ASSERT_NO_THROW(engine->cancel_transaction(0));
        XBT_INFO("cancel_transaction returned as a no-op as expected");
      });

      // Both T1 and T2 complete instantly with MQ; both current IDs are 2 before the canceller fires
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->end_transaction());
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->end_transaction());

      sg4::this_actor::sleep_for(2.0); // keep engine alive until canceller fires at 0.5s
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      (void)stream->inquire_variable("var");

      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->end_transaction());
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->end_transaction());
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Same scenario for the FileEngine: both sides complete two full transactions (including
// actual file I/O) so that both current_pub_transaction_id_ and current_sub_transaction_id_
// are > 0 when the canceller fires. cancel_transaction(0) must be a no-op and must call
// get_current_sub_transaction_impl() through the no-op guard of Engine::cancel_transaction().
TEST_F(DTLCancelTest, CancelNoOp_BothSidesPastTransaction_FileEngine)
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
      auto var    = stream->define_variable("var", {100}, {0}, {100}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(1.0);
        XBT_INFO("Both sides completed T1 and T2; cancel_transaction(0) must be a no-op");
        ASSERT_NO_THROW(engine->cancel_transaction(0));
        XBT_INFO("cancel_transaction returned as a no-op as expected");
      });

      ASSERT_NO_THROW(engine->begin_transaction());
      engine->put(var);
      ASSERT_NO_THROW(engine->end_transaction());
      ASSERT_NO_THROW(engine->begin_transaction());
      engine->put(var);
      ASSERT_NO_THROW(engine->end_transaction());

      sg4::this_actor::sleep_for(2.0); // keep engine alive until canceller fires at 1.0s
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      ASSERT_NO_THROW(engine->begin_transaction());
      engine->get(var_sub);
      ASSERT_NO_THROW(engine->end_transaction());
      ASSERT_NO_THROW(engine->begin_transaction());
      engine->get(var_sub);
      ASSERT_NO_THROW(engine->end_transaction());
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher and subscriber are both engaged in a long Mailbox transfer (Mailbox simulates bandwidth; MQ does not).
// Publisher completes T1 end_transaction() (starting slow async Comms) then blocks in T2 begin_transaction()
// waiting for T1 sends to complete. Subscriber blocks in T1 end_transaction() waiting for receives.
// Canceller fires after 0.5s, unblocking both with TransactionCanceledException.
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
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException in T2 begin_transaction() as expected");
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
      ASSERT_THROW(engine->end_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException in T1 end_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher begins T1 and then sleeps (simulating slow computation before end_transaction).
// Subscriber connects and immediately calls begin_transaction(), which blocks waiting for the publisher
// to signal pub_transaction_completed (StagingEngine lines 202-204).
// Canceller fires during that wait, unblocking the subscriber with TransactionCanceledException.
TEST_F(DTLCancelTest, CancelStagingTransaction_SubWaitingForPubToEndTx_MQ)
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
      [[maybe_unused]] auto var = stream->define_variable("var", {100}, {0}, {100}, sizeof(double));
      auto engine               = stream->open("my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      ASSERT_NO_THROW(engine->begin_transaction());
      sg4::this_actor::sleep_for(2.0); // hold T1 open long enough for sub to block on it
      ASSERT_NO_THROW(engine->end_transaction());
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl                      = dtlmod::DTL::connect();
      auto stream                   = dtl->add_stream("my-output");
      auto engine                   = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      [[maybe_unused]] auto var_sub = stream->inquire_variable("var");

      XBT_INFO("Subscriber calling begin_transaction() — will block waiting for pub to end T1");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Subscriber sleeps past an already-fired cancellation, then calls begin_transaction().
// FileEngine line 195: the early-exit check fires immediately on begin_sub_transaction()
// because canceled_transaction_id_ is already set before the subscriber enters.
TEST_F(DTLCancelTest, CancelFileEngineTransaction_SubAlreadyCanceled)
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
      [[maybe_unused]] auto var = stream->define_variable("var", {100}, {0}, {100}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(0.5);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      sg4::this_actor::sleep_for(3.0);
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      [[maybe_unused]] auto var_sub = stream->inquire_variable("var");

      sg4::this_actor::sleep_for(2.0); // sleep past the cancellation point
      XBT_INFO("Subscriber calling begin_transaction() after cancellation already fired");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher does T1 (put on slow disk), then immediately starts T2 begin_transaction(),
// which blocks waiting for T1 write activities to complete (FileEngine line 123).
// Canceller fires at 1s while writes are still in flight, unblocking publisher with
// TransactionCanceledException on T2 begin_transaction().
TEST_F(DTLCancelTest, CancelFileEngineTransaction_PubMidWrite)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_file_platform();
    auto* pub_host  = sg4::Host::by_name("node-0");
    auto* wdog_host = sg4::Host::by_name("node-1");

    pub_host->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      auto var    = stream->define_variable("var", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(1.0);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      // T1: begin + put (starts slow async writes), then end_transaction (returns immediately)
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->put(var));
      ASSERT_NO_THROW(engine->end_transaction());

      // T2: blocks waiting for T1 writes to finish on slow disk — canceled while waiting
      XBT_INFO("Begin T2 (will block waiting for T1 writes to complete on slow disk)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException in T2 begin_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher does T1 on slow disk; subscriber's end_sub_transaction() blocks waiting for the
// publisher's writes to complete (FileEngine lines 234-237: pub_activities_completed CV wait).
// Canceller fires at 1s while writes are still in flight, unblocking subscriber with
// TransactionCanceledException.
TEST_F(DTLCancelTest, CancelFileEngineTransaction_SubWaitingForPubWrites)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_file_platform();
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
        sg4::this_actor::sleep_for(1.0);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->put(var));
      ASSERT_NO_THROW(engine->end_transaction());
      sg4::this_actor::sleep_for(5.0);
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      // T1: begin and get succeed; end_transaction blocks waiting for pub writes — canceled there
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End T1 (will block waiting for pub writes to complete on slow disk)");
      ASSERT_THROW(engine->end_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException in T1 end_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Publisher writes 8MB to slow disk (~2.67s on 3MBps RAID5). Writes complete before the cancel at 4s.
// Subscriber starts reading after writes complete; canceller fires at 4s during the slow reads
// (FileEngine lines 31, 250-258). Subscriber's end_transaction() catches TransactionCanceledException.
TEST_F(DTLCancelTest, CancelFileEngineTransaction_SubMidRead)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_file_platform();
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
        sg4::this_actor::sleep_for(4.0); // after writes finish (~2.67s) but during reads
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->put(var));
      ASSERT_NO_THROW(engine->end_transaction());
      sg4::this_actor::sleep_for(10.0);
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      // T1: begin/get/end — end_transaction blocks during slow reads, canceled at 4s
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End T1 (will block waiting for reads to complete on slow disk)");
      ASSERT_THROW(engine->end_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException in T1 end_transaction() as expected");
      dtlmod::DTL::disconnect();
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// Regression test for the cancellation of an ActivitySet holding more than one activity.
//
// Four publishers own a quarter of the variable each, so the single subscriber posts four receives in the Engine's
// sub_transaction ActivitySet and blocks in end_transaction() on the slow link. Cancelling that transaction has to
// cancel the four of them: Activity::cancel() is a simcall, so the very first one resumes the subscriber, whose
// end_sub_transaction() clears the set. Cancelling while iterating the set itself therefore used to leave the
// activities it had not reached yet running, and clear() would then destroy them, orphaning their ActivityImpl in
// the kernel. A single publisher (as in the other cancellation tests of this file) cannot expose this.
TEST_F(DTLCancelTest, CancelStagingTransaction_MultiplePublishers_NoOrphanedActivity)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_staging_platform();

    for (size_t i = 0; i < 4; i++) {
      sg4::Host::by_name("host-" + std::to_string(i) + ".prod")->add_actor("Pub" + std::to_string(i), [i]() {
        auto dtl    = dtlmod::DTL::connect();
        auto stream = dtl->add_stream("my-output");
        stream->set_engine_type(dtlmod::Engine::Type::Staging);
        stream->set_transport_method(dtlmod::Transport::Method::Mailbox);
        auto var    = stream->define_variable("var", {1000, 1000}, {0, 250 * i}, {1000, 250}, sizeof(double));
        auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);

        XBT_INFO("Wait for all publishers to have opened the stream");
        sg4::this_actor::sleep_for(0.5);
        engine->begin_transaction();
        engine->put(var);
        engine->end_transaction();

        // The cancellation reaches the publishers on their next transaction. Which of them observes it is a race,
        // and is not what this test is about, so just let them all leave quietly.
        try {
          engine->begin_transaction();
        } catch (const dtlmod::TransactionCanceledException&) {
          XBT_INFO("Publisher caught TransactionCanceledException as expected");
        }
        dtlmod::DTL::disconnect();
      });
    }

    sg4::Host::by_name("host-0.cons")->add_actor("SubTestActor", []() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");

      sg4::Host::by_name("host-1.cons")->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(1.0);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      // No selection: this subscriber gets the whole variable, hence one receive per publisher.
      engine->begin_transaction();
      engine->get(var_sub);
      XBT_INFO("End T1 (will block on the four receives over the slow link)");
      ASSERT_THROW(engine->end_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Subscriber caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    // SimGrid reports an Activity destroyed while its implementation is still running, which is exactly what an
    // uncancelled leftover produces. Nothing else in this scenario can emit it.
    ::testing::internal::CaptureStderr();
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    std::string output = ::testing::internal::GetCapturedStderr();
    ASSERT_EQ(output.find("freed before its completion"), std::string::npos)
        << "an activity was destroyed without having been canceled:\n"
        << output;
  });
}

// Same defect, other mechanism, on the FileEngine side: cancelling a write fires its on_this_completion_cb, which
// erases it from the very ActivitySet being iterated over, so an indexed loop skipped every other write. Two
// variables are published in the same transaction to get two write activities for the single publisher; with one
// variable (as in CancelFileEngineTransaction_PubMidWrite) the skip cannot show up.
TEST_F(DTLCancelTest, CancelFileEngineTransaction_MultipleWrites_AllCanceled)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_slow_file_platform();
    auto* wdog_host = sg4::Host::by_name("node-1");

    sg4::Host::by_name("node-0")->add_actor("PubTestActor", [wdog_host]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_transport_method(dtlmod::Transport::Method::File);
      stream->set_engine_type(dtlmod::Engine::Type::File);
      auto var1   = stream->define_variable("var1", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto var2   = stream->define_variable("var2", {1000, 1000}, {0, 0}, {1000, 1000}, sizeof(double));
      auto engine = stream->open("cluster:my_fs:/node-0/scratch/my-output", dtlmod::Stream::Mode::Publish);

      wdog_host->add_actor("Canceller", [engine]() {
        sg4::this_actor::sleep_for(1.0);
        XBT_INFO("Cancelling the transaction");
        engine->cancel_transaction(engine->get_current_transaction());
      });

      // T1 starts two slow write activities, one per variable, and returns immediately.
      ASSERT_NO_THROW(engine->begin_transaction());
      ASSERT_NO_THROW(engine->put(var1));
      ASSERT_NO_THROW(engine->put(var2));
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Begin T2 (will block waiting for the T1 writes to complete on the slow disk)");
      ASSERT_THROW(engine->begin_transaction(), dtlmod::TransactionCanceledException);
      XBT_INFO("Publisher caught TransactionCanceledException as expected");
      dtlmod::DTL::disconnect();
    });

    // The two writes are the only Io activities of this scenario. Both must end up canceled: a write that the
    // cancellation skipped would instead keep running on the disk and complete normally after the cancellation point.
    int canceled              = 0;
    int finished_after_cancel = 0;
    sg4::Io::on_completion_cb([&canceled, &finished_after_cancel](sg4::Io const& io) {
      if (io.get_state() == sg4::Activity::State::CANCELED)
        canceled++;
      else if (io.get_state() == sg4::Activity::State::FINISHED && sg4::Engine::get_clock() > 1.0)
        finished_after_cancel++;
    });

    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
    ASSERT_EQ(canceled, 2) << "both pending writes should have been canceled";
    ASSERT_EQ(finished_after_cancel, 0) << "a write survived the cancellation and completed on its own";
  });
}
