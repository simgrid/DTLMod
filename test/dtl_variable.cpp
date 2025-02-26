/* Copyright (c) 2022-2024. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <gtest/gtest.h>
#include <simgrid/host.h>
#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_variable, "Logging category for this dtlmod test");

namespace sg4 = simgrid::s4u;

class DTLVariableTest : public ::testing::Test {
public:
  sg4::Disk* disk_;
  sg4::Host* host_;

  DTLVariableTest() = default;

  void setup_platform()
  {
    auto* root = sg4::Engine::get_instance()->get_netzone_root();
    host_      = root->add_host("host", "1Gf");
    disk_      = host_->add_disk("disk", "1kBps", "2kBps");
    root->seal();

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLVariableTest, DefineVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> scalar;
      std::shared_ptr<dtlmod::Variable> var3D;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a scalar int variable");
      ASSERT_NO_THROW(scalar = stream->define_variable("scalar", sizeof(int)));
      XBT_INFO("Create a 3D variable");
      ASSERT_NO_THROW(var3D = stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Check name");
      ASSERT_TRUE(var3D->get_name() == "var3D");
      XBT_INFO("Check size: should be 64^3 times 8 as elements are double");
      ASSERT_DOUBLE_EQ(var3D->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Remove variable named 'var3D'. It is known, should be true");
      ASSERT_DOUBLE_EQ(stream->remove_variable("var3D"), true);
      XBT_INFO("Remove variable named 'var2D'. It is unknown, should be false");
      ASSERT_DOUBLE_EQ(stream->remove_variable("var2D"), false);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, InconsistentVariableDefinition)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable with only two offsets, should fail.");
      ASSERT_THROW(stream->define_variable("var3D", {64, 64, 64}, {0, 0}, {64, 64, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a 3D variable with only two element counts, should fail.");
      ASSERT_THROW(stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, MultiDefineVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a scalar int variable");
      ASSERT_NO_THROW(stream->define_variable("var", sizeof(int)));
      XBT_INFO("Try to redefine var as a scalar double variable, which should fail");
      ASSERT_THROW(stream->define_variable("var", sizeof(double)), dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Try to redefine var as a 3D variable, which should fail");
      ASSERT_THROW(stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Define a new 3D variable");
      ASSERT_NO_THROW(stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Try to redefine var2 as a 2D variable, which should fail");
      ASSERT_THROW(stream->define_variable("var3D", {64, 64}, {0, 0}, {64, 64}, sizeof(double)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Try to redefine var as a 3D int variable, which should fail");
      ASSERT_THROW(stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(int)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Try to redefine starts and counts which should work");
      ASSERT_NO_THROW(var = stream->define_variable("var3D", {64, 64, 64}, {16, 16, 16}, {32, 32, 32}, sizeof(double)));
      XBT_INFO("Check local and global sizes");
      ASSERT_DOUBLE_EQ(var->get_local_size(), 32 * 32 * 32 * 8);
      ASSERT_DOUBLE_EQ(var->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, DistributedVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor1", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable");
      ASSERT_NO_THROW(var = stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {48, 48, 48}, sizeof(double)));
      XBT_INFO("Check local and global sizes");
      ASSERT_DOUBLE_EQ(var->get_local_size(), 48 * 48 * 48 * 8);
      ASSERT_DOUBLE_EQ(var->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    engine->add_actor("TestActor2", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable with a different shape which should fail");
      ASSERT_THROW(stream->define_variable("var", {64, 64}, {0, 0}, {64, 64}, sizeof(double)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Create a 3D variable with the same shape which should work");
      ASSERT_NO_THROW(var = stream->define_variable("var", {64, 64, 64}, {48, 48, 48}, {16, 16, 16}, sizeof(double)));
      XBT_INFO("Check local and global sizes");
      ASSERT_DOUBLE_EQ(var->get_local_size(), 16 * 16 * 16 * 8);
      ASSERT_DOUBLE_EQ(var->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, RemoveVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a scalar int variable");
      ASSERT_NO_THROW(var = stream->define_variable("var", sizeof(int)));
      XBT_INFO("Remove an existing variable, which should return true");
      ASSERT_TRUE(stream->remove_variable("var"));
      XBT_INFO("Remove an unknown variable, which should return false");
      ASSERT_FALSE(stream->remove_variable("unknow_var"));
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, InquireVariableLocal)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      std::shared_ptr<dtlmod::Variable> var2;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable");
      ASSERT_NO_THROW(var = stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Inquire this variable and store it var2");
      ASSERT_NO_THROW(var2 = stream->inquire_variable("var"));
      XBT_INFO("Check name and size of the inquired variable");
      ASSERT_TRUE(var2->get_name() == "var");
      ASSERT_DOUBLE_EQ(var2->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}

TEST_F(DTLVariableTest, InquireVariableRemote)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    auto* engine = sg4::Engine::get_instance();
    engine->add_actor("TestProducerActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable");
      ASSERT_NO_THROW(var = stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    engine->add_actor("TestConsumerActor", host_, [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Inquire the variable named 'var'");
      ASSERT_NO_THROW(var = stream->inquire_variable("var"));
      XBT_INFO("Check name and size of the inquired variable");
      ASSERT_TRUE(var->get_name() == "var");
      ASSERT_DOUBLE_EQ(var->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(engine->run());
  });
}
