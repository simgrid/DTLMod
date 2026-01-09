/* Copyright (c) 2024-2026. The SWAT Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef __DTLMOD_DTLEXCEPTION_HPP__
#define __DTLMOD_DTLEXCEPTION_HPP__

#include <simgrid/Exception.hpp>

#define DECLARE_DTLMOD_EXCEPTION(AnyException, msg_prefix, ...)                                                        \
  class AnyException : public simgrid::Exception {                                                                     \
  public:                                                                                                              \
    using simgrid::Exception::Exception;                                                                               \
    __VA_ARGS__                                                                                                        \
    AnyException(simgrid::xbt::ThrowPoint&& throwpoint, const std::string& message)                                    \
        : simgrid::Exception(std::move(throwpoint),                                                                    \
                             std::string(msg_prefix) + (message.empty() ? "" : std::string(": " + message)))           \
    {                                                                                                                  \
    }                                                                                                                  \
    explicit AnyException(simgrid::xbt::ThrowPoint&& throwpoint) : AnyException(std::move(throwpoint), "") {}          \
    ~AnyException() override = default;                                                                                \
    XBT_ATTRIB_NORETURN void rethrow_nested(simgrid::xbt::ThrowPoint&& throwpoint,                                     \
                                            const std::string& message) const override                                 \
    {                                                                                                                  \
      std::string augmented_message = std::string(msg_prefix) + message;                                               \
      std::throw_with_nested(AnyException(std::move(throwpoint), augmented_message));                                  \
    }                                                                                                                  \
  }

namespace dtlmod {

// All these exceptions derive from simgrid::Exception (but are in the dtlmod namespace)
DECLARE_DTLMOD_EXCEPTION(UnknownEngineTypeException, "Unknown Engine Type");
DECLARE_DTLMOD_EXCEPTION(TransportEngineMismatchException, "Transport and Engine mismatch");
DECLARE_DTLMOD_EXCEPTION(UndefinedEngineTypeException, "Undefined Engine Type. Cannot open Stream");
DECLARE_DTLMOD_EXCEPTION(MultipleEngineTypeException, "Stream can only use one engine type. Check your code.");

DECLARE_DTLMOD_EXCEPTION(UnknownTransportMethodException, "Unknown Transport Method");
DECLARE_DTLMOD_EXCEPTION(UndefinedTransportMethodException, "Undefined Transport Method. Cannot open Stream");
DECLARE_DTLMOD_EXCEPTION(MultipleTransportMethodException,
                         "Stream can only use one transport method. Check your code.");

DECLARE_DTLMOD_EXCEPTION(InvalidEngineAndTransportCombinationException,
                         "Invalid combination between Engine::Type and Transport::Method");
DECLARE_DTLMOD_EXCEPTION(OpenStreamFailureException, "Failed to open Stream");

DECLARE_DTLMOD_EXCEPTION(UnknownOpenModeException, "Unknown open mode. Should be Publish or Subscribe");

DECLARE_DTLMOD_EXCEPTION(UnknownVariableException, "Unknown Variable");
DECLARE_DTLMOD_EXCEPTION(MultipleVariableDefinitionException, "Multiple Variable Definition");
DECLARE_DTLMOD_EXCEPTION(InconsistentVariableDefinitionException, "Insconsistent Variable Definition");
DECLARE_DTLMOD_EXCEPTION(IncorrectPathDefinitionException, "Fullpath must be structured as follows: "
                                                           "netzone_name:file_system_name:/path/to/file_name");

DECLARE_DTLMOD_EXCEPTION(InvalidTransactionIdException,
                         "Impossible to get. This transaction doesn't exist for variable yet");
DECLARE_DTLMOD_EXCEPTION(GetWhenNoTransactionException, "Impossible to get. No transaction exists for variable");

DECLARE_DTLMOD_EXCEPTION(UnknownReductionMethodException,
                         "Unkown Reduction Method. Options are 'decimation' and 'compression'");
DECLARE_DTLMOD_EXCEPTION(InconsistentDecimationInterpolationException,
                         "Insconsistent Decimation Interpolation definition");
DECLARE_DTLMOD_EXCEPTION(InconsistentDecimationStrideException, "Insconsistent Decimation Stride definition");
DECLARE_DTLMOD_EXCEPTION(UnknownDecimationOptionException, "Unknown Decimation option");
DECLARE_DTLMOD_EXCEPTION(UnknownDecimationInterpolationException, "Unknown Decimation interpolation method");
DECLARE_DTLMOD_EXCEPTION(DoubleReductionException, "Double reduction is forbidden");

} // namespace dtlmod

#endif // __DTLMOD_EXCEPTION_HPP__
