/* Copyright (c) 2022-2025. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include "dtlmod/CompressionReductionMethod.hpp"
#include "dtlmod/DecimationReductionMethod.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/FileEngine.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Stream.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_stream, dtlmod, "DTL logging about Streams");

namespace dtlmod {

const char* Stream::get_engine_type_str() const
{
  const std::map<Engine::Type, const char*> EnumStrings{
      {Engine::Type::File, "Engine::Type::File"},
      {Engine::Type::Staging, "Engine::Type::Staging"},
      {Engine::Type::Undefined, "Engine::Type::Undefined"},
  };
  auto it = EnumStrings.find(engine_type_);
  return it == EnumStrings.end() ? "Out of range" : it->second;
}

Stream* Stream::set_engine_type(const Engine::Type& engine_type)
{
  if (engine_type_ == engine_type) // No modification, just return
    return this;

  // Check if this engine type is known
  if (engine_type != Engine::Type::File && engine_type != Engine::Type::Staging)
    throw UnknownEngineTypeException(XBT_THROW_POINT, "");

  // Check is one tries to redefine the engine type
  if (engine_type_ != Engine::Type::Undefined)
    throw MultipleEngineTypeException(XBT_THROW_POINT, "");

  // This is the first definition of the engine type.
  // Check for unauthorized combinations first.
  if (transport_method_ == Transport::Method::File && engine_type != Engine::Type::File)
    throw InvalidEngineAndTransportCombinationException(
        XBT_THROW_POINT, ": The Transport::Method::File transport method can only be used with Engine::File.");

  if ((transport_method_ == Transport::Method::Mailbox || transport_method_ == Transport::Method::MQ) &&
      engine_type != Engine::Type::Staging)
    throw InvalidEngineAndTransportCombinationException(
        XBT_THROW_POINT, ": The Transport::Method::Mailbox and Transport::Method::MQ transport methods "
                         "can only be used with Engine::Staging.");

  // set the engine type
  engine_type_ = engine_type;

  return this;
}

const char* Stream::get_transport_method_str() const
{
  const std::map<Transport::Method, const char*> EnumStrings{
      {Transport::Method::File, "Transport::Method::File"},
      {Transport::Method::Mailbox, "Transport::Method::Mailbox"},
      {Transport::Method::MQ, "Transport::Method::MQ"},
      {Transport::Method::Undefined, "Transport::Method::Undefined"},
  };
  auto it = EnumStrings.find(transport_method_);
  return it == EnumStrings.end() ? "Out of range" : it->second;
}

Stream* Stream::set_transport_method(const Transport::Method& transport_method)
{
  if (transport_method_ == transport_method) // No modification, just return
    return this;

  // Check if this transport method is known
  if (transport_method != Transport::Method::File && transport_method != Transport::Method::Mailbox &&
      transport_method != Transport::Method::MQ)
    throw UnknownTransportMethodException(XBT_THROW_POINT, "");

  // Check is one tries to redefine the transport method
  if (transport_method_ != Transport::Method::Undefined)
    throw MultipleTransportMethodException(XBT_THROW_POINT, "");

  // This is the first definition of the transport method.
  // Check for unauthorized combinations first.
  if (engine_type_ == Engine::Type::File && transport_method != Transport::Method::File)
    throw InvalidEngineAndTransportCombinationException(
        XBT_THROW_POINT, "An Engine::File only accepts Transport::Method::File as a transport method.");

  if (engine_type_ == Engine::Type::Staging &&
      not(transport_method == Transport::Method::Mailbox || transport_method == Transport::Method::MQ))
    throw InvalidEngineAndTransportCombinationException(
        XBT_THROW_POINT, "An Engine::Staging only accepts Transport::Method::Mailbox or Transport::Method::MQ as a "
                         "transport method.");

  // Set the transport method
  transport_method_ = transport_method;

  return this;
}

Stream* Stream::set_metadata_export()
{
  metadata_export_ = true;
  return this;
}
Stream* Stream::unset_metadata_export()
{
  metadata_export_ = false;
  return this;
}

std::shared_ptr<ReductionMethod> Stream::define_reduction_method(const std::string& name)
{
  std::shared_ptr<ReductionMethod> reduction_method;
  if (name == "Decimation" || name == "decimation")
    reduction_method = std::make_shared<DecimationReductionMethod>(name);
  else if (name == "Compression" || name == "compression")
    reduction_method = std::make_shared<CompressionReductionMethod>(name);  

  reduction_methods_.push_back(reduction_method);

  return reduction_method;
}


/****** Engine Factory ******/

/// When multiple actors open the same Stream, only the first one to call this function is in charge of creating an
/// Engine object for that Stream. The Engine creation is thus in a critical section. Each actor calling the open()
/// function is considered as a subscriber to the created Engine.
///
/// Both Engine::Type and Transport::Method have to be specified before opening a Stream.
///
/// For the FileEngine engine type, name corresponds to a fullpath to where to write data. This fullpath is
/// structured as follows: netzone_name:file_system_name:/path/to/file_name.
std::shared_ptr<Engine> Stream::open(const std::string& name, Mode mode)
{
  // Sanity checks
  if (engine_type_ == Engine::Type::Undefined)
    throw UndefinedEngineTypeException(XBT_THROW_POINT, name);
  if (transport_method_ == Transport::Method::Undefined)
    throw UndefinedTransportMethodException(XBT_THROW_POINT, name);
  if (mode != Mode::Publish && mode != Mode::Subscribe)
    throw UnknownOpenModeException(XBT_THROW_POINT, mode_to_str(mode));

  // Only the first Actor calling Stream::open has to create the corresponding Engine and Transport method.
  // Hence, we use a critical section. The creation of a FileEngine can raise an exception in the critical section
  // which requires special handling.
  bool got_exception = false;
  std::string exception_msg;

  dtl_->lock();
  if (not engine_) {
    if (engine_type_ == Engine::Type::Staging) {
      engine_ = std::make_shared<StagingEngine>(name, this);
      engine_->create_transport(transport_method_);
    } else if (engine_type_ == Engine::Type::File) {
      try {
        engine_ = std::make_shared<FileEngine>(name, this);
        engine_->create_transport(transport_method_);
      } catch (const IncorrectPathDefinitionException& e) {
        got_exception = true;
        exception_msg = std::string(e.what());
      }
    }
    access_mode_ = mode;
    if (metadata_export_)
      engine_->set_metadata_file_name();
  }
  dtl_->unlock();
  // Check if an exception has been raised and caugth while creating a FileEngine. If yes, throw it again.
  if (got_exception)
    throw IncorrectPathDefinitionException(XBT_THROW_POINT, exception_msg);

  while (not engine_) {
    XBT_DEBUG("%s waits for the creation of the engine", sg4::this_actor::get_cname());
    sg4::this_actor::sleep_for(0.001);
  }

  // Then we register the actors calling Stream::open as publishers or subscribers in the newly created Engine.
  if (mode == dtlmod::Stream::Mode::Publish) {
    engine_->add_publisher(sg4::Actor::self());
  } else {
    engine_->add_subscriber(sg4::Actor::self());
  }

  XBT_DEBUG("Stream '%s' uses engine '%s' and transport '%s' (%zu Pub. / %zu Sub.)", get_cname(), get_engine_type_str(),
            get_transport_method_str(), engine_->get_num_publishers(), engine_->get_num_subscribers());

  return engine_;
}

/****** Variable Factory ******/

/// This function creates a new scalar Variable and the corresponding entry in the internal directory of the Stream that
/// stores all the known variables. This definition does not refer to the data carried by the Variable but provides
/// information about its shape (here a scalar) and element type.
std::shared_ptr<Variable> Stream::define_variable(const std::string& name, size_t element_size)
{
  return define_variable(name, {1}, {0}, {1}, element_size);
}

/// This function creates a new Variable and the corresponding entry in the internal directory of the Stream that
/// stores all the known variables. This definition does not refer to the data carried by the Variable but provides
/// information about its shape (here a multi-dimensional array) and element type.
std::shared_ptr<Variable> Stream::define_variable(const std::string& name, const std::vector<size_t>& shape,
                                                  const std::vector<size_t>& start, const std::vector<size_t>& count,
                                                  size_t element_size)
{
  // Sanity checks
  // Vectors should have the same size
  if (shape.size() != start.size() || shape.size() != count.size())
    throw InconsistentVariableDefinitionException(
        XBT_THROW_POINT, std::string("Shape, Start, and Count vectors must have the same size. Shape: ") +
                             std::to_string(shape.size()) + ", Start: " + std::to_string(start.size()) +
                             ", Count: " + std::to_string(count.size()));

  // The local size of the variable (start + count) cannot exceed the global size (shape) of the Variable
  for (unsigned int i = 0; i < shape.size(); i++) {
    if (start[i] + count[i] > shape[i])
      throw InconsistentVariableDefinitionException(
          XBT_THROW_POINT, std::string("start + count is greater than the number of elements in shape for dimension") +
                               std::to_string(i));
  }

  std::unique_lock lock(*mutex_);
  auto publisher = sg4::Actor::self();
  auto var       = variables_.find(name);
  if (var != variables_.end()) {
    if (var->second->get_shape().size() != shape.size() || var->second->get_element_size() != element_size)
      throw MultipleVariableDefinitionException(XBT_THROW_POINT, name + " already exists in Stream " + get_name());
    else {
      var->second->set_local_start_and_count(publisher, std::make_pair(start, count));
      return var->second;
    }
  } else {
    auto new_var = std::make_shared<Variable>(name, element_size, shape);
    new_var->set_local_start_and_count(publisher, std::make_pair(start, count));
    new_var->create_metadata();
    variables_.try_emplace(name, new_var);
    return new_var;
  }
}

std::vector<std::string> Stream::get_all_variables() const
{
  std::vector<std::string> variable_names;
  variable_names.reserve(variables_.size());
  for (const auto& [name, var] : variables_)
    variable_names.push_back(name);
  return variable_names;
}

std::shared_ptr<Variable> Stream::inquire_variable(const std::string& name) const
{
  auto var = variables_.find(name);
  if (var == variables_.end())
    throw UnknownVariableException(XBT_THROW_POINT, name);

  auto actor = sg4::Actor::self();
  if (not engine_ || engine_->is_publisher(actor))
    return var->second;
  else {
    auto new_var = std::make_shared<Variable>(name, var->second->get_element_size(), var->second->get_shape());
    new_var->set_local_start_and_count(actor, std::make_pair(std::vector<size_t>(var->second->get_shape().size(), 0),
                                                             std::vector<size_t>(var->second->get_shape().size(), 0)));
    new_var->set_metadata(var->second->get_metadata());

    return new_var;
  }
}

bool Stream::remove_variable(const std::string& name)
{
  auto var = variables_.find(name);
  if (var != variables_.end()) {
    variables_.erase(var);
    return true;
  } else
    return false;
}

} // namespace dtlmod
