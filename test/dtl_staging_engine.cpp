/* Copyright (c) 2024-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_test_staging_engine, "Logging category for this dtlmod test");

class DTLStagingEngineTest : public ::testing::Test {
public:
  DTLStagingEngineTest() = default;

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

  void setup_platform()
  {
    auto* root     = sg4::Engine::get_instance()->get_netzone_root();
    auto* internet = root->add_link("internet", "500MBps")->set_latency("1ms");

    auto* prod_cluster = add_cluster(root, ".prod", 16);
    auto* cons_cluster = add_cluster(root, ".cons", 4);

    root->add_route(prod_cluster, cons_cluster, {internet});
    root->seal();

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLStagingEngineTest, SinglePubSingleSubSameCluster)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* pub_host = sg4::Host::by_name("host-0.prod");
    auto* sub_host = sg4::Host::by_name("host-0.cons");
  
    pub_host->add_actor("PubTestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::MQ);
      XBT_INFO("Create a 2D-array variable with 20kx20k double");
      auto var    = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Stream '%s' is ready for Publish data into the DTL", stream->get_cname());
      sg4::this_actor::sleep_for(1);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var));
      XBT_INFO("End the Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", [this]() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");
      ASSERT_TRUE(var_sub->get_name() == "var");
      auto shape   = var_sub->get_shape();
      ASSERT_TRUE(shape[0] == 20000 && shape[1] == 20000);
      ASSERT_DOUBLE_EQ(var_sub->get_global_size(), 8. * 20000 * 20000);

      XBT_INFO("Set a selection for 'var_sub': Just get the second half of the first dimension");
      ASSERT_NO_THROW(var_sub->set_selection({10000, 0}, {10000, shape[1]}));

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Get a subset of the Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End the Transaction");
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

TEST_F(DTLStagingEngineTest, MultiplePubSingleSubMessageQueue)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    
    std::vector<sg4::Host*> pub_hosts = {sg4::Host::by_name("host-0.prod"), sg4::Host::by_name("host-1.prod")};
    std::vector<sg4::Host*> sub_hosts = {sg4::Host::by_name("host-0.cons"), sg4::Host::by_name("host-0.cons")};

    for (long unsigned int i = 0; i < 2; i++) {
      pub_hosts[i]->add_actor("Pub" + std::to_string(i), [this, i]() {
        auto dtl    = dtlmod::DTL::connect();
        auto stream = dtl->add_stream("my-output");
        stream->set_engine_type(dtlmod::Engine::Type::Staging);
        stream->set_transport_method(dtlmod::Transport::Method::MQ);
        XBT_INFO("Create a 2D-array variable with 10kx10k double, publishers own 3/4 and 1/4 (along 2nd dimension)");
        auto var    = stream->define_variable("var", {10000, 10000}, {0, 2500 * 3 * i}, {10000, 7500 - (5000 * i)},
                                              sizeof(double));
        auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);
        XBT_INFO("Wait for all publishers to have opened the stream");
        sg4::this_actor::sleep_for(.5);
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

    for (long unsigned int i = 0; i < 2; i++) {
      sub_hosts[i]->add_actor("Sub" + std::to_string(i), [this, i]() {
        auto dtl     = dtlmod::DTL::connect();
        auto stream  = dtl->add_stream("my-output");
        auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
        auto var_sub = stream->inquire_variable("var");

        XBT_INFO("Set a selection for 'var_sub': get 3/4 and 1/4 of the first dimension respectively");
        ASSERT_NO_THROW(var_sub->set_selection({2500 * 3 * i, 0}, {7500 - (5000 * i), 10000}));
        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Get the Variable 'var_sub' from the DTL");
        ASSERT_NO_THROW(engine->get(var_sub));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());
        XBT_INFO("Check local size of var_sub.");
        ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 10000 * 10000 * (3 - 2 * i) / 4);

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

TEST_F(DTLStagingEngineTest, MultiplePubSingleSubMailbox)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    std::vector<sg4::Host*> pub_hosts = {sg4::Host::by_name("host-0.prod"), sg4::Host::by_name("host-1.prod")};
    std::vector<sg4::Host*> sub_hosts = {sg4::Host::by_name("host-0.cons"), sg4::Host::by_name("host-0.cons")};

    for (long unsigned int i = 0; i < 2; i++) {
      pub_hosts[i]->add_actor("Pub" + std::to_string(i), [this, i]() {
        auto dtl    = dtlmod::DTL::connect();
        auto stream = dtl->add_stream("my-output");
        stream->set_engine_type(dtlmod::Engine::Type::Staging);
        stream->set_transport_method(dtlmod::Transport::Method::Mailbox);
        XBT_INFO("Create a 2D-array variable with 10kx10k double, publishers own 3/4 and 1/4 (along 2nd dimension)");
        auto var    = stream->define_variable("var", {10000, 10000}, {0, 2500 * 3 * i}, {10000, 7500 - (5000 * i)},
                                              sizeof(double));
        auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);
        XBT_INFO("Wait for all publishers to have opened the stream");
        sg4::this_actor::sleep_for(.5);
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

    for (long unsigned int i = 0; i < 2; i++) {
      sub_hosts[i]->add_actor("Sub" + std::to_string(i), [this, i]() {
        auto dtl     = dtlmod::DTL::connect();
        auto stream  = dtl->add_stream("my-output");
        auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
        auto var_sub = stream->inquire_variable("var");

        XBT_INFO("Set a selection for 'var_sub': get 3/4 and 1/4 of the first dimension respectively");
        ASSERT_NO_THROW(var_sub->set_selection({2500 * 3 * i, 0}, {7500 - (5000 * i), 10000}));
        XBT_INFO("Start a Transaction");
        ASSERT_NO_THROW(engine->begin_transaction());
        XBT_INFO("Get the Variable 'var_sub' from the DTL");
        ASSERT_NO_THROW(engine->get(var_sub));
        XBT_INFO("End a Transaction");
        ASSERT_NO_THROW(engine->end_transaction());
        XBT_INFO("Check local size of var_sub.");
        ASSERT_DOUBLE_EQ(var_sub->get_local_size(), 8. * 10000 * 10000 * (3 - 2 * i) / 4);

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

TEST_F(DTLStagingEngineTest, MetadataExport)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* pub_host = sg4::Host::by_name("host-0.prod");
    auto* sub_host = sg4::Host::by_name("host-0.cons");

    pub_host->add_actor("PubTestActor", [this]() {
      auto dtl    = dtlmod::DTL::connect();
      auto stream = dtl->add_stream("my-output");
      stream->set_engine_type(dtlmod::Engine::Type::Staging);
      stream->set_transport_method(dtlmod::Transport::Method::MQ);
      XBT_INFO("Set metadata export for that stream");
      stream->set_metadata_export();

      XBT_INFO("Create a 2D-array variable with 20kx20k double");
      auto var    = stream->define_variable("var", {20000, 20000}, {0, 0}, {20000, 20000}, sizeof(double));
      auto engine = stream->open("my-output", dtlmod::Stream::Mode::Publish);
      XBT_INFO("Stream '%s' is ready for Publish data into the DTL", stream->get_cname());
      sg4::this_actor::sleep_for(1);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Put Variable 'var' into the DTL");
      ASSERT_NO_THROW(engine->put(var, var->get_local_size()));
      XBT_INFO("End the Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Get the name of the metadata file");
      auto metadata_file_name = stream->get_metadata_file_name();
      XBT_INFO("Check the contents of '%s'", metadata_file_name.c_str());
      std::ifstream file(metadata_file_name);
      ASSERT_TRUE(file.is_open());
      std::string file_contents((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

      file.close();
      const std::string& expected_contents =
        "8\tvar\t1*{20000,20000}\n"
        "  Transaction 1:\n"
        "    PubTestActor: [0:20000, 0:20000]\n";

      ASSERT_EQ(file_contents, expected_contents);
      std::remove(metadata_file_name.c_str());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    sub_host->add_actor("SubTestActor", [this]() {
      auto dtl     = dtlmod::DTL::connect();
      auto stream  = dtl->add_stream("my-output");
      auto engine  = stream->open("my-output", dtlmod::Stream::Mode::Subscribe);
      auto var_sub = stream->inquire_variable("var");
      ASSERT_TRUE(var_sub->get_name() == "var");
      auto shape   = var_sub->get_shape();
      ASSERT_TRUE(shape[0] == 20000 && shape[1] == 20000);
      ASSERT_DOUBLE_EQ(var_sub->get_global_size(), 8. * 20000 * 20000);

      XBT_INFO("Start a Transaction");
      ASSERT_NO_THROW(engine->begin_transaction());
      XBT_INFO("Get a the Variable 'var' from the DTL");
      ASSERT_NO_THROW(engine->get(var_sub));
      XBT_INFO("End the Transaction");
      ASSERT_NO_THROW(engine->end_transaction());

      XBT_INFO("Close the engine");
      ASSERT_NO_THROW(engine->close());

      XBT_INFO("Disconnect the actor");
      dtlmod::DTL::disconnect();
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
