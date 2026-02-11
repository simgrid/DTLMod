/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <cmath>
#include <gtest/gtest.h>
#include <simgrid/host.h>
#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include "./test_util.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"

XBT_LOG_NEW_DEFAULT_CATEGORY(dtlmod_test_variable, "Logging category for this dtlmod test");

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
    root->seal();

    // Create the DTL
    dtlmod::DTL::create();
  }
};

TEST_F(DTLVariableTest, DefineVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
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
      XBT_INFO("Check name of '%s'", var3D->get_cname());
      ASSERT_TRUE(var3D->get_name() == "var3D");
      XBT_INFO("Check size: should be 64^3 times 8 as elements are double");
      ASSERT_DOUBLE_EQ(var3D->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Remove variable named 'var3D'. It is known, should be fine");
      ASSERT_NO_THROW(stream->remove_variable("var3D"));
      XBT_INFO("Remove variable named 'var2D'. It is unknown, should fail");
      ASSERT_THROW(stream->remove_variable("var2D"), dtlmod::UnknownVariableException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, InconsistentVariableDefinition)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 1D variable with an element count bigger than the shape, should fail.");
      ASSERT_THROW((void)stream->define_variable("var", {64}, {0}, {128}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a 3D variable with only two offsets, should fail.");
      ASSERT_THROW((void)stream->define_variable("var3D", {64, 64, 64}, {0, 0}, {64, 64, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a 3D variable with only two element counts, should fail.");
      ASSERT_THROW((void)stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with empty shape vector, should fail.");
      ASSERT_THROW((void)stream->define_variable("varEmpty", {}, {}, {}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with zero dimension in shape, should fail.");
      ASSERT_THROW((void)stream->define_variable("varZeroShape", {64, 0, 64}, {0, 0, 0}, {64, 1, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with zero dimension in count, should fail.");
      ASSERT_THROW((void)stream->define_variable("varZeroCount", {64, 64, 64}, {0, 0, 0}, {64, 0, 64}, sizeof(double)),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with zero element_size, should fail.");
      ASSERT_THROW((void)stream->define_variable("varZeroElem", {64}, {0}, {64}, 0),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with wrapped negative in shape, should fail.");
      ASSERT_THROW(
          (void)stream->define_variable("varNegShape", {std::numeric_limits<size_t>::max()}, {0}, {1}, sizeof(double)),
          dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with wrapped negative in start, should fail.");
      ASSERT_THROW(
          (void)stream->define_variable("varNegStart", {64}, {std::numeric_limits<size_t>::max()}, {1}, sizeof(double)),
          dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with wrapped negative in count, should fail.");
      ASSERT_THROW(
          (void)stream->define_variable("varNegCount", {64}, {0}, {std::numeric_limits<size_t>::max()}, sizeof(double)),
          dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Create a variable with wrapped negative in element_size, should fail.");
      ASSERT_THROW((void)stream->define_variable("varNegElem", {64}, {0}, {64}, std::numeric_limits<size_t>::max()),
                   dtlmod::InconsistentVariableDefinitionException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, OverflowVariableSize)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      XBT_INFO("Connect to the DTL");
      auto dtl = dtlmod::DTL::connect();
      XBT_INFO("Create a stream");
      auto stream = dtl->add_stream("Stream");
      XBT_INFO("Define a variable whose dimensions overflow size_t when computing global size");
      auto var =
          stream->define_variable("huge", {std::numeric_limits<size_t>::max() / 2, 3}, {0, 0}, {1, 1}, sizeof(double));
      XBT_INFO("Calling get_global_size() should trigger an overflow exception");
      ASSERT_THROW(var->get_global_size(), std::overflow_error);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, MultiDefineVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a scalar int variable");
      ASSERT_NO_THROW((void)stream->define_variable("var", sizeof(int)));
      XBT_INFO("Try to redefine var as a scalar double variable, which should fail");
      ASSERT_THROW((void)stream->define_variable("var", sizeof(double)), dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Try to redefine var as a 3D variable, which should fail");
      ASSERT_THROW((void)stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Define a new 3D variable");
      ASSERT_NO_THROW((void)stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Try to redefine var2 as a 2D variable, which should fail");
      ASSERT_THROW((void)stream->define_variable("var3D", {64, 64}, {0, 0}, {64, 64}, sizeof(double)),
                   dtlmod::MultipleVariableDefinitionException);
      XBT_INFO("Try to redefine var as a 3D int variable, which should fail");
      ASSERT_THROW((void)stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(int)),
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
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, DistributedVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor1", [this]() {
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

    host_->add_actor("TestActor2", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable with a different shape which should fail");
      ASSERT_THROW((void)stream->define_variable("var", {64, 64}, {0, 0}, {64, 64}, sizeof(double)),
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
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, RemoveVariable)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a scalar int variable");
      ASSERT_NO_THROW(var = stream->define_variable("var", sizeof(int)));
      XBT_INFO("Remove variable named 'var'. It is known, should be fine");
      ASSERT_NO_THROW(stream->remove_variable("var"));
      XBT_INFO("Remove an unknown variable, which should throw");
      ASSERT_THROW(stream->remove_variable("unknow_var"), dtlmod::UnknownVariableException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, InquireVariableLocal)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestActor", [this]() {
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
      XBT_INFO("Inquire this variable and store it in var2");
      ASSERT_NO_THROW(var2 = stream->inquire_variable("var"));
      XBT_INFO("Check name and size of the inquired variable");
      ASSERT_TRUE(var2->get_name() == "var");
      ASSERT_DOUBLE_EQ(var2->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Inquire an unknown variable, which should raise an exception");
      ASSERT_THROW((void)stream->inquire_variable("unknow_var"), dtlmod::UnknownVariableException);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, InquireVariableRemote)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestProducerActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create a 3D variable");
      ASSERT_NO_THROW((void)stream->define_variable("var", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    host_->add_actor("TestConsumerActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::shared_ptr<dtlmod::Variable> var;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Inquire the variable named 'var' created by the producer");
      ASSERT_NO_THROW(var = stream->inquire_variable("var"));
      XBT_INFO("Check name and size of the inquired variable");
      ASSERT_TRUE(var->get_name() == "var");
      ASSERT_DOUBLE_EQ(var->get_global_size(), 64 * 64 * 64 * 8);
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}

TEST_F(DTLVariableTest, GetAllVariables)
{
  DO_TEST_WITH_FORK([this]() {
    this->setup_platform();
    host_->add_actor("TestProducerActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      XBT_INFO("Create 3 variables of different shapes");
      ASSERT_NO_THROW((void)stream->define_variable("var1D", {64}, {0}, {64}, sizeof(double)));
      ASSERT_NO_THROW((void)stream->define_variable("var2D", {64, 64}, {0, 0}, {64, 64}, sizeof(double)));
      ASSERT_NO_THROW((void)stream->define_variable("var3D", {64, 64, 64}, {0, 0, 0}, {64, 64, 64}, sizeof(double)));
      XBT_INFO("Disconnect the actor from the DTL");
      ASSERT_NO_THROW(dtlmod::DTL::disconnect());
    });

    host_->add_actor("TestConsumerActor", [this]() {
      std::shared_ptr<dtlmod::DTL> dtl;
      std::shared_ptr<dtlmod::Stream> stream;
      std::vector<std::string> var_list;
      XBT_INFO("Connect to the DTL");
      ASSERT_NO_THROW(dtl = dtlmod::DTL::connect());
      sg4::this_actor::sleep_for(1);
      XBT_INFO("Create a stream");
      ASSERT_NO_THROW(stream = dtl->add_stream("Stream"));
      ASSERT_NO_THROW(var_list = stream->get_all_variables());
      ASSERT_EQ(var_list.size(), 3U);
      XBT_INFO("Inquire the different variables created by the producer");
      double ndims = 3.0;
      for (const auto& var_name : var_list) {
        std::shared_ptr<dtlmod::Variable> var;
        ASSERT_NO_THROW(var = stream->inquire_variable(var_name));
        XBT_INFO("Check name and size of the inquired variable: %s", var_name.c_str());
        ASSERT_TRUE(var->get_name() == var_name);
        ASSERT_DOUBLE_EQ(var->get_global_size(), pow(64, ndims) * 8);
        XBT_INFO("Disconnect the actor from the DTL");
        ASSERT_NO_THROW(dtlmod::DTL::disconnect());
        ndims--;
      }
    });

    // Run the simulation
    ASSERT_NO_THROW(sg4::Engine::get_instance()->run());
  });
}
