/* Copyright (c) 2022-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#include <array>
#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <fstream>

#include "dtlmod/CompressionReductionMethod.hpp"
#include "dtlmod/DTL.hpp"
#include "dtlmod/DTLException.hpp"
#include "dtlmod/DecimationReductionMethod.hpp"
#include "dtlmod/FileEngine.hpp"
#include "dtlmod/ReductionMethod.hpp"
#include "dtlmod/StagingEngine.hpp"
#include "dtlmod/Stream.hpp"
#include "dtlmod/Variable.hpp"

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(dtlmod_stream, dtlmod, "DTL logging about Streams");

namespace dtlmod {

// Constexpr lookup table for Engine::Type to string conversion
constexpr std::array<std::pair<Engine::Type, const char*>, 3> engine_type_strings{{
    {Engine::Type::File, "Engine::Type::File"},
    {Engine::Type::Staging, "Engine::Type::Staging"},
    {Engine::Type::Undefined, "Engine::Type::Undefined"},
}};

// Constexpr lookup table for Transport::Method to string conversion
constexpr std::array<std::pair<Transport::Method, const char*>, 4> transport_method_strings{{
    {Transport::Method::File, "Transport::Method::File"},
    {Transport::Method::Mailbox, "Transport::Method::Mailbox"},
    {Transport::Method::MQ, "Transport::Method::MQ"},
    {Transport::Method::Undefined, "Transport::Method::Undefined"},
}};

// Constexpr validators for compile-time checking
namespace {
constexpr bool is_valid_engine_type(Engine::Type type) noexcept
{
  return type == Engine::Type::File || type == Engine::Type::Staging;
}

constexpr bool is_valid_transport_method(Transport::Method method) noexcept
{
  return method == Transport::Method::File || method == Transport::Method::Mailbox || method == Transport::Method::MQ;
}

constexpr bool is_valid_mode(Stream::Mode mode) noexcept
{
  return mode == Stream::Mode::Publish || mode == Stream::Mode::Subscribe;
}
} // namespace

std::optional<const char*> Stream::get_engine_type_str() const noexcept
{
  for (const auto& [type, str] : engine_type_strings) {
    if (type == engine_type_)
      return str;
  }
  return std::nullopt; // LCOV_EXCL_LINE
}

Stream& Stream::set_engine_type(const Engine::Type& engine_type)
{
  if (engine_type_ == engine_type) // No modification, just return
    return *this;

  // Check if this engine type is known
  if (!is_valid_engine_type(engine_type))
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

  return *this;
}

std::optional<const char*> Stream::get_transport_method_str() const noexcept
{
  for (const auto& [method, str] : transport_method_strings) {
    if (method == transport_method_)
      return str;
  }
  return std::nullopt; // LCOV_EXCL_LINE
}

Stream& Stream::set_transport_method(const Transport::Method& transport_method)
{
  if (transport_method_ == transport_method) // No modification, just return
    return *this;

  // Check if this transport method is known
  if (!is_valid_transport_method(transport_method))
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

  return *this;
}

Stream& Stream::set_metadata_export() noexcept
{
  metadata_export_ = true;
  return *this;
}
Stream& Stream::unset_metadata_export() noexcept
{
  metadata_export_ = false;
  return *this;
}

void Stream::export_metadata_to_file() const
{
  if (metadata_export_) {
    std::ofstream export_stream(metadata_file_, std::ofstream::out);
    for (const auto& [name, v] : variables_)
      v->get_metadata()->export_to_file(export_stream);
    export_stream.close();
  }
}

std::shared_ptr<ReductionMethod> Stream::define_reduction_method(const std::string& name)
{
  if (auto it = reduction_methods_.find(name); it != reduction_methods_.end())
    return it->second;

  std::shared_ptr<ReductionMethod> reduction_method;
  if (name == "decimation")
    reduction_method = std::make_shared<DecimationReductionMethod>(name);
  else if (name == "compression")
    reduction_method = std::make_shared<CompressionReductionMethod>(name);
  else
    throw UnknownReductionMethodException(XBT_THROW_POINT, name);

  reduction_methods_.try_emplace(name, reduction_method);

  return reduction_method;
}


/****** Engine Factory ******/

/// Validate that all required parameters are set before opening a Stream.
void Stream::validate_open_parameters(std::string_view name, Mode mode) const
{
  if (engine_type_ == Engine::Type::Undefined)
    throw UndefinedEngineTypeException(XBT_THROW_POINT, std::string(name));
  if (transport_method_ == Transport::Method::Undefined)
    throw UndefinedTransportMethodException(XBT_THROW_POINT, std::string(name));
  if (!is_valid_mode(mode))
    throw UnknownOpenModeException(XBT_THROW_POINT, mode_to_str(mode));
}

/// Create the Engine if this is the first actor opening the Stream.
/// Executed in a critical section to ensure only one Engine is created.
void Stream::create_engine_if_needed(std::string_view name, Mode mode)
{
  std::scoped_lock lock(*dtl_->mutex_);

  if (not engine_) {
    std::shared_ptr<Engine> temp_engine;

    if (engine_type_ == Engine::Type::Staging) {
      temp_engine = std::make_shared<StagingEngine>(name, shared_from_this());
      temp_engine->create_transport(transport_method_);
    } else if (engine_type_ == Engine::Type::File) {
      temp_engine = std::make_shared<FileEngine>(name, shared_from_this());
      temp_engine->create_transport(transport_method_);
    }

    // Only commit if fully initialized
    engine_      = std::move(temp_engine);
    access_mode_ = mode;
    if (metadata_export_)
      metadata_file_ = boost::replace_all_copy(engine_->get_name(), "/", "#") + "#md." +
                       std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
  }
}

/// Register the current actor as a publisher or subscriber with the Engine.
void Stream::register_actor_with_engine(Mode mode) const
{
  if (mode == Mode::Publish)
    engine_->add_publisher(sg4::Actor::self());
  else
    engine_->add_subscriber(sg4::Actor::self());
}

/// When multiple actors open the same Stream, only the first one to call this function is in charge of creating an
/// Engine object for that Stream. The Engine creation is thus in a critical section. Each actor calling the open()
/// function is considered as a subscriber to the created Engine.
///
/// Both Engine::Type and Transport::Method have to be specified before opening a Stream.
///
/// For the FileEngine engine type, name corresponds to a fullpath to where to write data. This fullpath is
/// structured as follows: netzone_name:file_system_name:/path/to/file_name.
std::shared_ptr<Engine> Stream::open(std::string_view name, Mode mode)
{
  validate_open_parameters(name, mode);
  create_engine_if_needed(name, mode);
  register_actor_with_engine(mode);
  XBT_DEBUG("Stream '%s' uses engine '%s' and transport '%s' (%zu Pub. / %zu Sub.)", get_cname(),
            get_engine_type_str().value_or("Unknown"), get_transport_method_str().value_or("Unknown"),
            engine_->get_publishers().count(), engine_->get_subscribers().count());
  return engine_;
}

/****** Variable Factory ******/

/// This function creates a new scalar Variable and the corresponding entry in the internal directory of the Stream that
/// stores all the known variables. This definition does not refer to the data carried by the Variable but provides
/// information about its shape (here a scalar) and element type.
std::shared_ptr<Variable> Stream::define_variable(std::string_view name, size_t element_size)
{
  return define_variable(name, {1}, {0}, {1}, element_size);
}

/// Validate variable parameters for shape, start, count, and element_size.
/// Checks for empty vectors, size mismatches, zero dimensions, and wrapped negative numbers.
void Stream::validate_variable_parameters(const std::vector<size_t>& shape, const std::vector<size_t>& start,
                                          const std::vector<size_t>& count, size_t element_size)
{
  // Check for empty vectors
  if (shape.empty())
    throw InconsistentVariableDefinitionException(XBT_THROW_POINT, "Shape vector cannot be empty");

  // Vectors should have the same size
  if (shape.size() != start.size() || shape.size() != count.size())
    throw InconsistentVariableDefinitionException(
        XBT_THROW_POINT, std::string("Shape, Start, and Count vectors must have the same size. Shape: ") +
                             std::to_string(shape.size()) + ", Start: " + std::to_string(start.size()) +
                             ", Count: " + std::to_string(count.size()));

  // Check for zero dimensions and detect wrapped negative numbers
  // size_t is unsigned, so negative values wrap to very large numbers (> SIZE_MAX/2)
  constexpr size_t max_reasonable_dim = std::numeric_limits<size_t>::max() / 2;

  for (unsigned int i = 0; i < shape.size(); i++) {
    if (shape[i] == 0)
      throw InconsistentVariableDefinitionException(XBT_THROW_POINT, std::string("Shape dimension ") +
                                                                         std::to_string(i) + " cannot be zero");

    if (shape[i] > max_reasonable_dim)
      throw InconsistentVariableDefinitionException(
          XBT_THROW_POINT,
          std::string("Shape dimension ") + std::to_string(i) +
              " has suspiciously large value (possible wrapped negative): " + std::to_string(shape[i]));

    if (start[i] > max_reasonable_dim)
      throw InconsistentVariableDefinitionException(
          XBT_THROW_POINT,
          std::string("Start dimension ") + std::to_string(i) +
              " has suspiciously large value (possible wrapped negative): " + std::to_string(start[i]));

    if (count[i] == 0)
      throw InconsistentVariableDefinitionException(XBT_THROW_POINT, std::string("Count dimension ") +
                                                                         std::to_string(i) + " cannot be zero");

    if (count[i] > max_reasonable_dim)
      throw InconsistentVariableDefinitionException(
          XBT_THROW_POINT,
          std::string("Count dimension ") + std::to_string(i) +
              " has suspiciously large value (possible wrapped negative): " + std::to_string(count[i]));
  }

  // Check element_size
  if (element_size == 0)
    throw InconsistentVariableDefinitionException(XBT_THROW_POINT, "Element size cannot be zero");

  if (element_size > max_reasonable_dim)
    throw InconsistentVariableDefinitionException(
        XBT_THROW_POINT, std::string("Element size has suspiciously large value (possible wrapped negative): ") +
                             std::to_string(element_size));

  // The local size of the variable (start + count) cannot exceed the global size (shape) of the Variable
  // Check written to avoid overflow: count[i] > (shape[i] - start[i])
  for (unsigned int i = 0; i < shape.size(); i++) {
    if (start[i] > shape[i] || count[i] > shape[i] - start[i])
      throw InconsistentVariableDefinitionException(
          XBT_THROW_POINT, std::string("start + count exceeds shape in dimension ") + std::to_string(i) +
                               " (start: " + std::to_string(start[i]) + ", count: " + std::to_string(count[i]) +
                               ", shape: " + std::to_string(shape[i]) + ")");
  }
}

/// This function creates a new Variable and the corresponding entry in the internal directory of the Stream that
/// stores all the known variables. This definition does not refer to the data carried by the Variable but provides
/// information about its shape (here a multi-dimensional array) and element type.
std::shared_ptr<Variable> Stream::define_variable(std::string_view name, const std::vector<size_t>& shape,
                                                  const std::vector<size_t>& start, const std::vector<size_t>& count,
                                                  size_t element_size)
{
  // Validate parameters
  validate_variable_parameters(shape, start, count, element_size);

  std::unique_lock lock(*mutex_);
  auto publisher = sg4::Actor::self();
  std::string name_str(name);
  auto var = variables_.find(name_str);
  if (var != variables_.end()) {
    if (var->second->get_shape().size() != shape.size() || var->second->get_element_size() != element_size)
      throw MultipleVariableDefinitionException(XBT_THROW_POINT, name_str + " already exists in Stream " + get_name());
    else {
      var->second->set_local_start_and_count(publisher, std::make_pair(start, count));
      return var->second;
    }
  } else {
    auto new_var = std::make_shared<Variable>(name_str, element_size, shape, shared_from_this());
    new_var->set_local_start_and_count(publisher, std::make_pair(start, count));
    new_var->create_metadata();
    variables_.try_emplace(name_str, new_var);
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
} // LCOV_EXCL_LINE

std::shared_ptr<Variable> Stream::inquire_variable(std::string_view name) const
{
  std::string name_str(name);
  auto var = variables_.find(name_str);
  if (var == variables_.end())
    throw UnknownVariableException(XBT_THROW_POINT, name_str);

  auto actor = sg4::Actor::self();
  if (not engine_ || engine_->get_publishers().contains(actor))
    return var->second;
  else {
    auto new_var = std::make_shared<Variable>(name_str, var->second->get_element_size(), var->second->get_shape(),
                                              shared_from_this());
    new_var->set_local_start_and_count(actor, std::make_pair(std::vector<size_t>(var->second->get_shape().size(), 0),
                                                             std::vector<size_t>(var->second->get_shape().size(), 0)));
    new_var->set_metadata(var->second->get_metadata());

    // Propagate reduction state so subscribers can detect publisher-side reduction
    if (var->second->is_reduced()) {
      new_var->is_reduced_with_  = var->second->get_reduction_method();
      new_var->reduction_origin_ = var->second->reduction_origin_;
    }

    return new_var;
  }
}

void Stream::remove_variable(std::string_view name)
{
  std::string name_str(name);
  auto var = variables_.find(name_str);
  if (var != variables_.end())
    variables_.erase(var);
  else
    throw UnknownVariableException(XBT_THROW_POINT, name_str);
}

} // namespace dtlmod
