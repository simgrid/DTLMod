/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>
#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_connection, "Logging category for this dtlmod test");

namespace sg4 = simgrid::s4u;

class DTLConnectionTest : public ::testing::Test {
public:
  DTLConnectionTest() = default;
  std::vector<sg4::Host*> hosts_;

  void setup_platform()
  {
    auto* cluster = sg4::create_star_zone("cluster");
    for (int i = 0; i < 4; i++) {
      std::string hostname = std::string("node-") + std::to_string(i);

      const auto* host = cluster->create_host(hostname, "1Gf");

      std::string linkname = std::string("cluster") + "_link_" + std::to_string(i);
      auto* link_up        = cluster->create_link(linkname + "_UP", "1Gbps");
      auto* link_down      = cluster->create_link(linkname + "_DOWN", "1Gbps");
      auto* loopback =
          cluster->create_link(hostname + "_loopback", "10Gbps")->set_sharing_policy(sg4::Link::SharingPolicy::FATPIPE);

      cluster->add_route(host, nullptr, {sg4::LinkInRoute(link_up)}, false);
      cluster->add_route(nullptr, host, {sg4::LinkInRoute(link_down)}, false);
      cluster->add_route(host, host, {loopback});
    }
    cluster->seal();
    hosts_ = sg4::Engine::get_instance()->get_all_hosts();

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLConnectionTest, SyncConSyncDecon)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    for (int i = 0; i < 4; i++) {
      sg4::Actor::create(std::string("client-") + std::to_string(i), hosts_[i], [this]() {
        std::shared_ptr<dtlmod::DTL> dtl;
        XBT_INFO("Connect to the DTL");
        ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
        XBT_INFO("Let the actor sleep for 1 second");
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
      });
    }
    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLConnectionTest, AsyncConSyncDecon)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    for (int i = 0; i < 4; i++) {
      sg4::Actor::create(std::string("client-") + std::to_string(i), hosts_[i], [this, i]() {
        std::shared_ptr<dtlmod::DTL> dtl;
        XBT_INFO("Let actor %s sleep for %.1f second", sg4::this_actor::get_cname(), 0.1 * i);
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(0.1 * i));
        XBT_INFO("Connect to the DTL");
        ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
        XBT_INFO("Let actor %s sleep for %.1f second", sg4::this_actor::get_cname(), 1 - (0.1 * i));
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(1 - (0.1 * i)));
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
      });
    }
    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLConnectionTest, SyncConAsyncDecon)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    for (int i = 0; i < 4; i++) {
      sg4::Actor::create(std::string("client-") + std::to_string(i), hosts_[i], [this, i]() {
        std::shared_ptr<dtlmod::DTL> dtl;
        XBT_INFO("Connect to the DTL");
        ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
        XBT_INFO("Let actor %s sleep for %.1f second", sg4::this_actor::get_cname(), 0.1 * i);
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(0.1 * i));
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
      });
    }

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLConnectionTest, AsyncConAsyncDecon)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    for (int i = 0; i < 4; i++) {
      sg4::Actor::create(std::string("client-") + std::to_string(i), hosts_[i], [this, i]() {
        std::shared_ptr<dtlmod::DTL> dtl;
        XBT_INFO("Connect to the DTL");
        XBT_INFO("Let actor %s sleep for %.1f second", sg4::this_actor::get_cname(), 0.1 * i);
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(0.1 * i));
        ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
        XBT_INFO("Let actor %s sleep for 1 second", sg4::this_actor::get_cname());
        ASSERT_NO_THROW(sg4::this_actor::sleep_for(1));
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
      });
    }

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

// static void client(unsigned int id)
// {

//   if (id == 0)
//     XBT_INFO("[Test 5] Missing disconnection");

//   dtlmod::DTL::connect();
//   sg4::this_actor::sleep_for(1);
//   if (id != 0)
//     dtlmod::DTL::disconnect();
//   sg4::this_actor::sleep_for(1);

//   if (id == 0)
//     XBT_INFO("[Test 6] Double connection");

//   dtlmod::DTL::connect();
//   sg4::this_actor::sleep_for(1);
//   dtlmod::DTL::disconnect();
//   sg4::this_actor::sleep_for(1);

//   if (id == 0)
//    XBT_INFO("[Test 7] Double disconnection");

//   if (id == 3)
//     dtlmod::DTL::disconnect();
// }
